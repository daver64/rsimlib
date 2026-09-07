#include "game.h"
#include "entity.h"

#include <algorithm>
#include <vector>

namespace game
{
    namespace
    {
        std::vector<GameObject> playing_objects;
        std::vector<GameObject> playing_objects_initial;
        bool playing_objects_initialised = false;

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
            blue.restitution = 0.6f;
            green.restitution = 0.8f;

            playing_objects = {red, blue, green};
            playing_objects_initial = playing_objects;
        }

        void reset_playing_objects()
        {
            playing_objects = playing_objects_initial;
        }
    }

    void handle_playing_input(SDL_Event event)
    {
        switch (event.type)
        {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        current_mode = Mode::menu;
                        break;
                    case SDLK_SPACE:
                        reset_playing_objects();
                        break;
                }
                break;
        }
    }

    void update_and_render_playing()
    {
        ensure_playing_objects_initialised();

        // clamp dt so a slow/paused frame doesn't cause a huge physics jump
        const float dt_seconds = std::min(0.05f, static_cast<float>(simlib::get_frame_time()) / 1000.0f);
        physics_step(playing_objects, dt_seconds);
        resolve_collisions(playing_objects);
        constrain_to_screen(playing_objects);

        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});

        simlib::gprintf_center(1+fontheight,text_colour,  "Playing Mode");

        render_objects(playing_objects);

        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}
