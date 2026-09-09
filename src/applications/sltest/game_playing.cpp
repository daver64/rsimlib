#include "game.h"
#include "entity.h"
#include "particles.h"
#include "graphics_fx.h"
#include "noise.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <vector>

namespace game
{
    namespace
    {
        std::vector<GameObject> playing_objects;
        std::vector<GameObject> playing_objects_initial;
        bool playing_objects_initialised = false;
        std::size_t active_balloon_index = 0;
        bool active_thrust_left = false;
        bool active_thrust_right = false;
        std::uint64_t active_thrust_voice = 0;
        std::uint64_t active_burner_voice = 0;
        simlib::Bitmap *playing_scene = nullptr;
        simlib::Vignette playing_vignette;
        constexpr float active_thrust_acceleration = 180.0f;

        bool ensure_playing_post_process()
        {
            const int width = simlib::screen_width();
            const int height = simlib::screen_height();
            if (width <= 0 || height <= 0)
            {
                return false;
            }
            if (!playing_scene || playing_scene->width != width || playing_scene->height != height)
            {
                simlib::destroy_bitmap(playing_scene);
                playing_scene = simlib::create_render_target(width, height);
            }
            if (!playing_vignette.is_valid())
            {
                playing_vignette.initialise();
                playing_vignette.set_radius(0.65f);
                playing_vignette.set_softness(0.35f);
                playing_vignette.set_intensity(0.65f);
            }
            return playing_scene != nullptr && playing_vignette.is_valid();
        }

        void update_active_thrust_sound()
        {
            const bool thrust_active = active_thrust_left || active_thrust_right;
            if (thrust_active && thrust_sound && active_thrust_voice == 0)
            {
                active_thrust_voice = simlib::play_sample(thrust_sound, 128, 128, 1000, -1);
            }
            else if (!thrust_active && active_thrust_voice != 0)
            {
                simlib::stop_voice(active_thrust_voice);
                active_thrust_voice = 0;
            }
        }

        void update_active_burner_sound(const GameObject &active_object)
        {
            if (active_object.burner_active && burner_sound && active_burner_voice == 0)
            {
                active_burner_voice = simlib::play_sample(burner_sound, 128, 128, 1000, -1);
            }
            else if (!active_object.burner_active && active_burner_voice != 0)
            {
                simlib::stop_voice(active_burner_voice);
                active_burner_voice = 0;
            }
        }

        /** @brief Return the index of the balloon whose collider contains (x, y), or -1 if none. */
        int find_balloon_at(float x, float y)
        {
            for (std::size_t i = 0; i < playing_objects.size(); ++i)
            {
                const GameObject &object = playing_objects[i];
                if (!object.is_balloon)
                {
                    continue;
                }
                const float dx = x - object.x;
                const float dy = y - object.y;
                if (dx * dx + dy * dy <= object.radius * object.radius)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        /** @brief Build a static dirt/grass ground strip whose height follows a FastNoiseLite profile. */
        std::vector<GameObject> build_terrain()
        {
            constexpr float block_size = 32.0f;
            constexpr int min_height_blocks = 2;
            constexpr int max_height_blocks = 6;

            std::vector<GameObject> blocks;
            simlib::Generator noise{1337};
            noise.set_frequency(0.08f);

            const int screen_w = simlib::screen_width();
            const int screen_h = simlib::screen_height();
            const int columns = static_cast<int>(std::ceil(screen_w / block_size)) + 1;
            for (int column = 0; column < columns; ++column)
            {
                const float normalized = (noise.get(static_cast<float>(column), 0.0f) + 1.0f) * 0.5f;
                const int height_blocks = min_height_blocks +
                    static_cast<int>(normalized * (max_height_blocks - min_height_blocks + 1));
                const float column_x = column * block_size + block_size * 0.5f;
                for (int row = 0; row < height_blocks; ++row)
                {
                    const float block_y = screen_h - block_size * 0.5f - row * block_size;
                    GameObject block = make_aabb_object(
                        row == height_blocks - 1 ? grass_texture : dirt_texture,
                        column_x, block_y, block_size, block_size);
                    block.is_static = true;
                    blocks.push_back(block);
                }
            }
            return blocks;
        }

        /**
         * @brief Lazily create physics objects, emitters, and their particle render target.
         *
         * This avoids allocating mode-local resources until gameplay is actually entered and
         * preserves a copy of the initial objects for the Space-key reset action.
         */
        void ensure_playing_objects_initialised()
        {
            if (playing_objects_initialised)
            {
                return;
            }
            playing_objects_initialised = true;

            struct BalloonSpec { float x; float mass; float restitution; float drag; float gas_bag_volume; };
            const BalloonSpec specs[] = {
                {100.0f, 1.0f, 0.2f, 0.5f, 1.2f},
                {400.0f, 1.5f, 0.1f, 0.7f, 1.8f},
                {600.0f, 1.0f, 0.2f, 0.5f, 1.1f},
            };

            playing_objects.clear();
            for (std::size_t i = 0; i < std::size(specs) && i < balloon_textures.size(); ++i)
            {
                GameObject balloon = make_circle_object(
                    balloon_textures[i], specs[i].x, 120.0f, 16.0f, specs[i].mass);
                balloon.restitution = specs[i].restitution;
                balloon.is_balloon = true;
                balloon.drag = specs[i].drag;
                balloon.gas_bag_volume = specs[i].gas_bag_volume;
                playing_objects.push_back(balloon);
            }
            const std::vector<GameObject> terrain = build_terrain();
            playing_objects.insert(playing_objects.end(), terrain.begin(), terrain.end());
            playing_objects_initial = playing_objects;
        }

        /** @brief Restore the initial physics snapshot, discard particles, and reselect the red balloon. */
        void reset_playing_objects()
        {
            playing_objects = playing_objects_initial;
            active_balloon_index = 0;
            active_thrust_left = false;
            active_thrust_right = false;
            simlib::stop_voice(active_thrust_voice);
            active_thrust_voice = 0;
            simlib::stop_voice(active_burner_voice);
            active_burner_voice = 0;

        }
    }

    void shutdown_playing()
    {
        simlib::destroy_bitmap(playing_scene);
        playing_scene = nullptr;
        playing_vignette.shutdown();
    }

    /** @brief Handle gameplay reset, balloon selection, and return-to-menu actions. */
    void handle_playing_input(const simlib::Event &event)
    {
        if (event.type() == simlib::Event::Type::mouse_button_down)
        {
            const int hit = find_balloon_at(
                static_cast<float>(simlib::mouse_x()), static_cast<float>(simlib::mouse_y()));
            if (hit >= 0)
            {
                active_balloon_index = static_cast<std::size_t>(hit);
            }
            return;
        }

        if (playing_objects.empty() || active_balloon_index >= playing_objects.size())
        {
            return;
        }

        GameObject &active_object = playing_objects[active_balloon_index];
        switch (event.type())
        {
            case simlib::Event::Type::key_down:
                switch (event.key())
                {
                    case simlib::Event::Key::escape:
                        request_mode(Mode::menu);
                        break;
                    case simlib::Event::Key::space:
                        reset_playing_objects();
                        break;
                    case simlib::Event::Key::plus:
                    case simlib::Event::Key::equals:
                    case simlib::Event::Key::keypad_plus:
                        adjust_balloon_volume(active_object, 0.1f);
                        break;
                    case simlib::Event::Key::minus:
                    case simlib::Event::Key::keypad_minus:
                        adjust_balloon_volume(active_object, -0.1f);
                        break;
                    case simlib::Event::Key::letter_b:
                        active_object.burner_active = true;
                        update_active_burner_sound(active_object);
                        break;
                    case simlib::Event::Key::arrow_left:
                        active_thrust_left = true;
                        update_active_thrust_sound();
                        break;
                    case simlib::Event::Key::arrow_right:
                        active_thrust_right = true;
                        update_active_thrust_sound();
                        break;
                    case simlib::Event::Key::f11:
                        simlib::toggle_fullscreen();
                        break;
                }
                break;
            case simlib::Event::Type::key_up:
                if (event.key() == simlib::Event::Key::letter_b)
                {
                    active_object.burner_active = false;
                    update_active_burner_sound(active_object);
                }
                else if (event.key() == simlib::Event::Key::arrow_left)
                {
                    active_thrust_left = false;
                    update_active_thrust_sound();
                }
                else if (event.key() == simlib::Event::Key::arrow_right)
                {
                    active_thrust_right = false;
                    update_active_thrust_sound();
                }
                break;
        }
    }

    /**
     * @brief Advance physics and particle emitters, then render the gameplay scene.
     *
     * The particle systems render to a simlib offscreen target before its vertically flipped
     * texture is composited over balloon sprites and text on the display framebuffer.
     */
    void update_and_render_playing()
    {
        ensure_playing_objects_initialised();

        bool post_process_ready = ensure_playing_post_process();
        if (post_process_ready)
        {
            post_process_ready = simlib::begin_render_target(playing_scene);
            if (post_process_ready)
            {
                simlib::clear_render_target(simlib::Colour{45, 48, 56});
            }
        }

        // clamp dt so a slow/paused frame doesn't cause a huge physics jump
        const float dt_seconds = std::min(0.05f, static_cast<float>(simlib::get_frame_time()) / 1000.0f);
        if (active_balloon_index < playing_objects.size())
        {
            GameObject &active_object = playing_objects[active_balloon_index];
            const float thrust_direction = (active_thrust_right ? 1.0f : 0.0f) - (active_thrust_left ? 1.0f : 0.0f);
            active_object.vx += thrust_direction * active_thrust_acceleration * dt_seconds;
        }
        physics_step(playing_objects, dt_seconds);
        resolve_collisions(playing_objects);
        constrain_to_screen(playing_objects);

        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        if (!post_process_ready)
        {
            simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});
        }
        if (playing_background)
        {
            simlib::draw_sprite_stretched(
                playing_background, 0.0f, 0.0f,
                simlib::screen_width(), simlib::screen_height());
        }
        simlib::rectfill(simlib::screen,simlib::screen_width()/6,0,
            simlib::screen_width()*5/6, 
            1+10*fontheight, 
            simlib::Colour{45, 48, 56, 128});  
        simlib::gprintf_center(1+fontheight,text_colour,  "Playing Mode");
        simlib::gprintf_center(1+2*fontheight,text_colour,  "Press SPACE to reset");
        simlib::gprintf_center(1+3*fontheight,text_colour,  "Click a balloon to select it");
        simlib::gprintf_center(1+4*fontheight,text_colour,  "+/-: selected balloon volume, hold B: burner");
        simlib::gprintf_center(1+5*fontheight,text_colour,  "Left/right: selected balloon thrust; layered winds alternate direction");
        simlib::gprintf_center(1+6*fontheight,text_colour,  "M: toggle music, F11: toggle fullscreen");
        simlib::gprintf_center(1+7*fontheight,text_colour,  "Press ESC to return to menu");

        render_objects(playing_objects);
        if (active_balloon_index < playing_objects.size())
        {
            const GameObject &active_object = playing_objects[active_balloon_index];
            simlib::rect(
                simlib::screen,
                active_object.x - active_object.radius, active_object.y - active_object.radius,
                active_object.x + active_object.radius, active_object.y + active_object.radius,
                simlib::Colour{0, 255, 0});
            const int label_x = static_cast<int>(active_object.x + active_object.radius) + 4;
            const int label_y = static_cast<int>(active_object.y - active_object.radius);
            const float altitude = simlib::screen_height() - active_object.y;
            char burner_line[64];
            char volume_line[64];
            char altitude_line[64];
            std::snprintf(burner_line, sizeof(burner_line), "Burner: %s", active_object.burner_active ? "active" : "off");
            std::snprintf(volume_line, sizeof(volume_line), "Gas volume: %.1f", active_object.gas_bag_volume);
            std::snprintf(altitude_line, sizeof(altitude_line), "Altitude: %.0f", altitude);
            const int label_width = std::max({
                simlib::text_length(font, burner_line),
                simlib::text_length(font, volume_line),
                simlib::text_length(font, altitude_line)});
            simlib::rectfill(
                simlib::screen,
                label_x - 4, label_y,
                label_x + label_width + 4, label_y + 3 * fontheight,
                simlib::Colour{45, 48, 56, 128});
            simlib::gprintf(label_x, label_y, text_colour, "%s", burner_line);
            simlib::gprintf(label_x, label_y + fontheight, text_colour, "%s", volume_line);
            simlib::gprintf(label_x, label_y + 2 * fontheight, text_colour, "%s", altitude_line);
        }

        if (post_process_ready)
        {
            simlib::end_render_target();
            playing_vignette.apply(playing_scene, 0, 0,
                simlib::screen_width(), simlib::screen_height(), true);
        }
        apply_mode_fade();
        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}
