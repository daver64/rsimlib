#include "game.h"
#include "entity.h"
#include "particles.h"
#include "graphics_fx.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace game
{
    namespace
    {
        std::vector<GameObject> playing_objects;
        std::vector<GameObject> playing_objects_initial;
        bool playing_objects_initialised = false;
        ///simlib::ParticleEmitter fountain_emitter;
        //simlib::ParticleEmitter trail_emitter;
        //simlib::Bitmap *particle_target = nullptr;

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

            GameObject red = make_circle_object(red_balloon, 100.0f, 100.0f, 16.0f, 1.0f);
            GameObject blue = make_circle_object(blue_balloon, 400.0f, 100.0f, 16.0f, 1.5f);
            GameObject green = make_circle_object(green_balloon, 600.0f, 400.0f, 16.0f, 1.0f);
            red.restitution = 0.2f;
            blue.restitution = 0.1f;
            green.restitution = 0.8f;
            red.is_balloon = true;
            blue.is_balloon = true;
            green.is_balloon = true;
            red.drag = 0.5f;
            blue.drag = 0.7f;
            green.drag = 0.4f;
            red.gas_bag_volume=1.2;
            blue.gas_bag_volume=1.8;
            green.gas_bag_volume=1.5;
            playing_objects = {red, blue, green};
            playing_objects_initial = playing_objects;
/*
            // static fountain: fixed position, sprays upward
            simlib::EmitterConfig fountain_config;
            fountain_config.emit_rate = 60.0f;
            fountain_config.particle_lifetime = 1.2f;
            fountain_config.min_speed = 80.0f;
            fountain_config.max_speed = 560.0f;
            fountain_config.direction_degrees = -90.0f;
            fountain_config.spread_degrees = 20.0f;
            fountain_config.gravity = 300.0f;
            fountain_config.start_size = 6.0f;
            fountain_config.end_size = 1.0f;
            fountain_config.start_colour = simlib::Colour{255, 220, 80, 255};
            fountain_config.end_colour = simlib::Colour{255, 60, 0, 0};
            fountain_emitter = simlib::ParticleEmitter(fountain_config, 200.0f, 550.0f);

            // moving emitter: follows the red balloon every frame, like a trail
            simlib::EmitterConfig trail_config;
            trail_config.emit_rate = 40.0f;
            trail_config.particle_lifetime = 0.5f;
            trail_config.min_speed = 5.0f;
            trail_config.max_speed = 20.0f;
            trail_config.spread_degrees = 180.0f;
            trail_config.gravity = 0.0f;
            trail_config.start_size = 10.0f;
            trail_config.end_size = 0.0f;
            trail_config.start_colour = simlib::Colour{120, 200, 255, 200};
            trail_config.end_colour = simlib::Colour{120, 200, 255, 0};
            trail_emitter = simlib::ParticleEmitter(trail_config, red.x, red.y);

            particle_target = simlib::create_render_target(simlib::screen_width(), simlib::screen_height());
*/
        }

        /** @brief Restore the initial physics snapshot and discard particles from both emitters. */
        void reset_playing_objects()
        {
            playing_objects = playing_objects_initial;
            //fountain_emitter.clear();
            //trail_emitter.clear();
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
                }
                break;
            case simlib::Event::Type::key_up:
                if (event.key() == simlib::Event::Key::letter_b)
                {
                    red_balloon_object.burner_active = false;
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
        if (playing_objects.size() > 1)
        {
            GameObject &blue_balloon_object = playing_objects[1];
            const float phase = static_cast<float>(simlib::time_ms()) * 0.001f;
            const float target_volume = 1.85f + 0.05f * std::sin(phase);
            adjust_balloon_volume(
                blue_balloon_object,
                target_volume - blue_balloon_object.gas_bag_volume);
        }
        physics_step(playing_objects, dt_seconds);
        resolve_collisions(playing_objects);
        constrain_to_screen(playing_objects);

        /*if (!playing_objects.empty())
        {
            trail_emitter.set_position(playing_objects[0].x, playing_objects[0].y);
        }
        fountain_emitter.update(dt_seconds);
        trail_emitter.update(dt_seconds);

        // render particles into their own offscreen target, isolated from the rest of the scene
        if (particle_target && simlib::begin_render_target(particle_target))
        {
            simlib::clear_render_target(simlib::Colour{0, 0, 0, 0});
            fountain_emitter.render();
            trail_emitter.render();
            simlib::end_render_target();
        }*/

        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});

        simlib::gprintf_center(1+fontheight,text_colour,  "Playing Mode");
        simlib::gprintf_center(1+2*fontheight,text_colour,  "Press SPACE to reset");
        simlib::gprintf_center(1+3*fontheight,text_colour,  "+/-: red balloon volume, hold B: burner");
        simlib::gprintf_center(1+4*fontheight,text_colour,  "Four layered winds alternate direction; balloons wrap horizontally");
        simlib::gprintf_center(1+5*fontheight,text_colour,  "Press ESC to return to menu");
        if (!playing_objects.empty())
        {
            const GameObject &red_balloon_object = playing_objects.front();
            simlib::gprintf_center(
                1+6*fontheight, text_colour, "Volume: %.1f  Gas: %.0f K%s",
                red_balloon_object.gas_bag_volume, red_balloon_object.gas_temperature,
                red_balloon_object.burner_active ? "  BURNING" : "");
        }
        
        render_objects(playing_objects);

        // composite the offscreen particle target back over the scene; render targets
        // sample bottom-up, so this always needs the vertical flip
        //if (particle_target)
        //{
        //    simlib::draw_sprite_v_flip(particle_target, 0.0f, 0.0f);
        //}

        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}
