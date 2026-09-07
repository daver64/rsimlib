#include "game.h"

namespace game
{
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
                }
                break;
        }
    }

    void update_and_render_playing()
    {
        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});

        simlib::gprintf_center(1+fontheight,text_colour,  "Playing Mode");
        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}
