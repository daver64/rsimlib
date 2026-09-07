#include "game.h"


namespace game
{
    std::atomic<bool> running{true};
    Mode current_mode{Mode::menu};
    void gprintf(int x, int y, simlib::Colour colour, const char *fmt, ...)
    {
        simlib::Font *font = simlib::get_default_monospace_font();
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        simlib::textout(font, x, y, colour, buffer);
    }

    void shutdown()
    {
        simlib::gui_shutdown();
        simlib::display_shutdown();
        simlib::audio_fx_shutdown();
    }

    bool handle_events()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                running = false;
            }
            simlib::display_handle_event(event);
            simlib::gui_handle_event(event);
        }
        return true;
    }

    bool initialise()
    {
        if (!simlib::set_gfx_mode(simlib::GFX_AUTODETECT_WINDOWED, 800, 600))
        {
            return false;
        }
        current_mode = Mode::menu;
        simlib::gui_init();
        simlib::set_fps(60);
        return true;
    }

    void update_and_render()
    {
        switch (current_mode)
        {
        case Mode::menu:
            update_and_render_menu();
            break;
        case Mode::playing:
            update_and_render_playing();
            break;
        case Mode::paused:
            update_and_render_paused();
            break;
        case Mode::help:
            update_and_render_help();
            break;
        case Mode::gameover:
            update_and_render_gameover();
            break;
        case Mode::settings:
            update_and_render_settings();
            break;
        }

    }
}
