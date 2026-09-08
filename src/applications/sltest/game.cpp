#include "game.h"

namespace game
{
    std::atomic<bool> running{true};
    Mode current_mode{Mode::menu};

    simlib::Bitmap* red_balloon = nullptr;
    simlib::Bitmap* blue_balloon = nullptr;
    simlib::Bitmap* green_balloon = nullptr;    
    simlib::Bitmap* playing_background = nullptr;
    simlib::Sample* thrust_sound = nullptr;
    simlib::Sample* burner_sound = nullptr;
    simlib::Stream* background_music = nullptr;
    bool background_music_paused = false;

    /** @brief Release game-owned resources before their dependent simlib subsystems. */
    void shutdown()
    {
        shutdown_lua_console();
        simlib::destroy_bitmap(playing_background);
        simlib::destroy_bitmap(green_balloon);
        simlib::destroy_bitmap(blue_balloon);
        simlib::destroy_bitmap(red_balloon);
        simlib::destroy_stream(background_music);
        simlib::destroy_sample(thrust_sound);
        simlib::destroy_sample(burner_sound);
        simlib::gamepad_shutdown();
        simlib::gui_shutdown();
        simlib::display_shutdown();
        simlib::audio_fx_shutdown();
        simlib::music_shutdown();
        simlib::shutdown();
    }

    /** @brief Return the flag controlled by quit events and menu/Lua quit actions. */
    bool is_running()
    {
        return running;
    }
    /** @brief Poll SDL, route mode input, and notify display and ImGui backends of each event. */
    bool handle_events()
    {
        simlib::Event event;
        while (simlib::poll_event(&event))
        {
            if (event.type() == simlib::Event::Type::quit)
            {
                running = false;
            }
            else if (event.type() == simlib::Event::Type::key_down &&
                     !event.key_repeat() && event.key() == simlib::Event::Key::letter_m &&
                     background_music)
            {
                background_music_paused = !background_music_paused;
                if (background_music_paused)
                {
                    simlib::pause_stream();
                }
                else
                {
                    simlib::resume_stream();
                }
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
            simlib::gamepad_handle_event(event);
            simlib::gui_handle_event(event);
        }
        return true;
    }

    /** @brief Create the window and initialise the simlib services used by the sample game. */
    bool initialise()
    {
        if (!simlib::set_gfx_mode(simlib::GFX_AUTODETECT_WINDOWED, 800, 600))
        {
            return false;
        }
        current_mode = Mode::menu;
        simlib::gui_init();
        if (!simlib::gamepad_init())
        {
            return false;
        }
        simlib::set_fps(60);
        simlib::music_init();
        background_music = simlib::load_stream("assets/music/Solar Serenity.ogg");
        if (background_music)
        {
            simlib::music_set_volume(20);
            simlib::play_stream(background_music);
        }
        simlib::audio_fx_init();
        thrust_sound = simlib::load_sample("assets/sfx/sustain.wav");
        burner_sound = simlib::load_sample("assets/sfx/engines.wav");
        SDL_StopTextInput();

        red_balloon = simlib::load_bitmap("assets/textures/balloon_red.png");
        blue_balloon = simlib::load_bitmap("assets/textures/balloon_blue.png");
        green_balloon = simlib::load_bitmap("assets/textures/balloon_green.png");
        playing_background = simlib::load_bitmap("assets/textures/Bumpy_Sky-Blue_01-512x512.png");
        return true;
    }

    /** @brief Select the current mode's complete update-and-render operation. */
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
