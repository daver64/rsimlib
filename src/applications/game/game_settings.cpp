#include "game.h"

namespace game
{
    void handle_settings_input(SDL_Event event)
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
    void update_and_render_settings()
    {
        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});
        simlib::gprintf_center(1+fontheight,text_colour,  "Settings");
        simlib::gprintf_center(1+3*fontheight,text_colour,"1....Option 1");
        simlib::gprintf_center(1+5*fontheight,text_colour,"2....Option 2");
        simlib::gprintf_center(1+7*fontheight,text_colour,"3....Option 3");
        simlib::gprintf_center(1+9*fontheight,text_colour,"ESC..Back    ");
        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}
