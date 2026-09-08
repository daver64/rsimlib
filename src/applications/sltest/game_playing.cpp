#include "game.h"
#include "entity.h"
#include "particles.h"
#include "graphics_fx.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace game
{
    namespace
    {
        std::vector<GameObject> playing_objects;
        std::vector<GameObject> playing_objects_initial;
        bool playing_objects_initialised = false;
        std::mt19937 random_engine{std::random_device{}()};
        std::uniform_real_distribution<float> green_idle_duration{10.0f, 25.0f};
        std::uniform_real_distribution<float> green_burn_duration{4.0f, 6.0f};
        float green_burner_timer = 0.0f;
        float green_burner_duration = 0.0f;
        bool green_burner_active = false;
        bool red_thrust_left = false;
        bool red_thrust_right = false;
        constexpr float red_thrust_acceleration = 180.0f;

        void reset_green_burner_cycle()
        {
            green_burner_active = false;
            green_burner_timer = 0.0f;
            green_burner_duration = green_idle_duration(random_engine);
        }

        void update_green_burner(float dt_seconds)
        {
            if (playing_objects.size() < 3)
            {
                return;
            }

            green_burner_timer += dt_seconds;
            if (green_burner_timer >= green_burner_duration)
            {
                green_burner_active = !green_burner_active;
                green_burner_timer = 0.0f;
                green_burner_duration = green_burner_active
                    ? green_burn_duration(random_engine)
                    : green_idle_duration(random_engine);
            }
            playing_objects[2].burner_active = green_burner_active;
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

            GameObject red = make_circle_object(red_balloon, 100.0f, simlib::screen_height()-16, 16.0f, 1.0f);
            GameObject blue = make_circle_object(blue_balloon, 400.0f, simlib::screen_height()-16, 16.0f, 1.5f);
            GameObject green = make_circle_object(green_balloon, 600.0f, simlib::screen_height()-16, 16.0f, 1.0f);
            red.restitution = 0.2f;
            blue.restitution = 0.1f;
            green.restitution = 0.2f;
            red.is_balloon = true;
            blue.is_balloon = true;
            green.is_balloon = true;
            red.drag = 0.5f;
            blue.drag = 0.7f;
            green.drag = 0.5f;
            red.gas_bag_volume=1.2;
            blue.gas_bag_volume=1.8;
            green.gas_bag_volume=1.1;
            playing_objects = {red, blue, green};
            playing_objects_initial = playing_objects;
            reset_green_burner_cycle();
        }

        /** @brief Restore the initial physics snapshot and discard particles from both emitters. */
        void reset_playing_objects()
        {
            playing_objects = playing_objects_initial;
            reset_green_burner_cycle();
            red_thrust_left = false;
            red_thrust_right = false;

        }
    }

    /** @brief Handle gameplay reset and return-to-menu keyboard actions. */
    void handle_playing_input(const simlib::Event &event)
    {
        if (playing_objects.empty())
        {
            return;
        }

        GameObject &red_balloon_object = playing_objects.front();
        switch (event.type())
        {
            case simlib::Event::Type::key_down:
                switch (event.key())
                {
                    case simlib::Event::Key::escape:
                        current_mode = Mode::menu;
                        break;
                    case simlib::Event::Key::space:
                        reset_playing_objects();
                        break;
                    case simlib::Event::Key::plus:
                    case simlib::Event::Key::equals:
                    case simlib::Event::Key::keypad_plus:
                        adjust_balloon_volume(red_balloon_object, 0.1f);
                        break;
                    case simlib::Event::Key::minus:
                    case simlib::Event::Key::keypad_minus:
                        adjust_balloon_volume(red_balloon_object, -0.1f);
                        break;
                    case simlib::Event::Key::letter_b:
                        red_balloon_object.burner_active = true;
                        break;
                    case simlib::Event::Key::arrow_left:
                        red_thrust_left = true;
                        break;
                    case simlib::Event::Key::arrow_right:
                        red_thrust_right = true;
                        break;
                    case simlib::Event::Key::f11:
                        simlib::toggle_fullscreen();
                        break;
                }
                break;
            case simlib::Event::Type::key_up:
                if (event.key() == simlib::Event::Key::letter_b)
                {
                    red_balloon_object.burner_active = false;
                }
                else if (event.key() == simlib::Event::Key::arrow_left)
                {
                    red_thrust_left = false;
                }
                else if (event.key() == simlib::Event::Key::arrow_right)
                {
                    red_thrust_right = false;
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

        // clamp dt so a slow/paused frame doesn't cause a huge physics jump
        const float dt_seconds = std::min(0.05f, static_cast<float>(simlib::get_frame_time()) / 1000.0f);
        if (!playing_objects.empty())
        {
            GameObject &red_balloon_object = playing_objects.front();
            const float thrust_direction = (red_thrust_right ? 1.0f : 0.0f) - (red_thrust_left ? 1.0f : 0.0f);
            red_balloon_object.vx += thrust_direction * red_thrust_acceleration * dt_seconds;
        }
        if (playing_objects.size() > 1)
        {
            GameObject &blue_balloon_object = playing_objects[1];
            const float phase = static_cast<float>(simlib::time_ms()) * 0.001f;
            const float target_volume = 2.2f + 0.02f * std::sin(phase);
            adjust_balloon_volume(
                blue_balloon_object,
                target_volume - blue_balloon_object.gas_bag_volume);
        }
        update_green_burner(dt_seconds);
        physics_step(playing_objects, dt_seconds);
        resolve_collisions(playing_objects);
        constrain_to_screen(playing_objects);

        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});

        simlib::gprintf_center(1+fontheight,text_colour,  "Playing Mode");
        simlib::gprintf_center(1+2*fontheight,text_colour,  "Press SPACE to reset");
        simlib::gprintf_center(1+3*fontheight,text_colour,  "+/-: red balloon volume, hold B: burner");
        simlib::gprintf_center(1+4*fontheight,text_colour,  "Left/right: red balloon thrust; layered winds alternate direction");
        simlib::gprintf_center(1+5*fontheight,text_colour,  "Press ESC to return to menu");
        if (!playing_objects.empty())
        {
            const GameObject &red_balloon_object = playing_objects.front();
            simlib::gprintf_center(
                1+6*fontheight, text_colour, "Volume: %.1f  Gas: %.0f K%s",
                red_balloon_object.gas_bag_volume, red_balloon_object.gas_temperature,
                red_balloon_object.burner_active ? "  BURNING" : "");
        }
        if (playing_objects.size() > 2)
        {
            simlib::gprintf_center(
                1+7*fontheight, text_colour, "Green burner: %s (%.1f s remaining)",
                green_burner_active ? "BURNING" : "idle",
                std::max(0.0f, green_burner_duration - green_burner_timer));
        }
        
        render_objects(playing_objects);

        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}
