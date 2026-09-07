#include "game.h"

namespace game
{
    std::atomic<bool> running{true};
    Mode current_mode{Mode::menu};

    simlib::Bitmap* red_balloon = nullptr;
    simlib::Bitmap* blue_balloon = nullptr;
    simlib::Bitmap* green_balloon = nullptr;    

    void shutdown()
    {
        simlib::gui_shutdown();
        simlib::display_shutdown();
        simlib::audio_fx_shutdown();
        simlib::music_shutdown();
        SDL_Quit();
    }

    bool is_running()
    {
        return running;
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

            switch (current_mode)
            {
            case Mode::menu:
                handle_menu_input(event);
                break;
            case Mode::playing:
                handle_playing_input(event);
                break;
            case Mode::paused:
                handle_paused_input(event);
                break;
            case Mode::help:
                handle_help_input(event);
                break;
            case Mode::gameover:
                handle_gameover_input(event);
                break;
            case Mode::settings:
                handle_settings_input(event);
                break;
            case Mode::lua_console:
                handle_lua_console_input(event);
                break;
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
        simlib::music_init();
        simlib::audio_fx_init();
        SDL_StopTextInput();

        red_balloon = simlib::load_bitmap("assets/textures/balloon_red.png");
        blue_balloon = simlib::load_bitmap("assets/textures/balloon_blue.png");
        green_balloon = simlib::load_bitmap("assets/textures/balloon_green.png");
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
        case Mode::lua_console:
            update_and_render_lua_console();
            break;
        }
    }
}
