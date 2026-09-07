#include "game.h"

namespace game
{
    void handle_menu_input(SDL_Event event)
    {

        switch (event.type)
        {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                case SDLK_1:
                    current_mode = Mode::playing;
                    break;
                case SDLK_2:
                    current_mode = Mode::settings;
                    break;
                case SDLK_3:
                    current_mode = Mode::help;
                    break;
                case SDLK_ESCAPE:
                    running = false;
                    break;
                }
                break;
        }

    }

    void update_and_render_menu()
    {
        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});

        gprintf_center(1+fontheight,text_colour,  "Menu");
        gprintf_center(1+3*fontheight,text_colour,"1....Play    ");
        gprintf_center(1+5*fontheight,text_colour,"2....Settings");
        gprintf_center(1+7*fontheight,text_colour,"3....Help    ");
        gprintf_center(1+9*fontheight,text_colour,"ESC..Quit    "); 


        //const double frame_time = simlib::get_frame_time();
        //const int x = 1;
        //const int y = 1;
        //gprintf(x, y + fontheight,
        //        text_colour, "frame time: %.2f ms (%.1f fps)",
        //        frame_time, frame_time > 0.0 ? 1000.0 / frame_time : 0.0);


        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}
