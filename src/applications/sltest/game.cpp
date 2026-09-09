#include "game.h"

#include <algorithm>

namespace game
{
    std::atomic<bool> running{true};
    Mode current_mode{Mode::menu};

    std::vector<simlib::Bitmap*> balloon_textures;
    simlib::Bitmap* playing_background = nullptr;
    simlib::Bitmap* dirt_texture = nullptr;
    simlib::Bitmap* grass_texture = nullptr;
    simlib::Sample* thrust_sound = nullptr;
    simlib::Sample* burner_sound = nullptr;
    simlib::Stream* background_music = nullptr;
    bool background_music_paused = false;
    bool background_music_started = false;
    bool background_music_active = false;

    namespace
    {
        Mode pending_mode = Mode::menu;
        bool mode_fading_out = false;
        bool mode_fading_in = false;
        float mode_fade_timer = 0.0f;
        constexpr float mode_fade_duration = 0.2f;
        std::uint8_t mode_fade_alpha = 0;
        simlib::ScreenFade mode_fade;
    }

    /** @brief Begin a cross-fade to @p mode instead of switching current_mode immediately. */
    void request_mode(Mode mode)
    {
        if (mode == current_mode && !mode_fading_out && !mode_fading_in)
        {
            return;
        }
        pending_mode = mode;
        mode_fading_out = true;
        mode_fading_in = false;
        mode_fade_timer = 0.0f;
    }

    /** @brief Advance the mode-transition timer, swapping current_mode once the fade-out completes. */
    void update_mode_fade()
    {
        if (!mode_fading_out && !mode_fading_in)
        {
            mode_fade_alpha = 0;
            return;
        }
        const float dt_seconds = static_cast<float>(simlib::get_frame_time()) / 1000.0f;
        mode_fade_timer += dt_seconds;
        const float progress = std::clamp(mode_fade_timer / mode_fade_duration, 0.0f, 1.0f);
        const float alpha = mode_fading_out ? progress : 1.0f - progress;
        mode_fade_alpha = static_cast<std::uint8_t>(alpha * 255.0f);
        if (progress < 1.0f)
        {
            return;
        }
        if (mode_fading_out)
        {
            current_mode = pending_mode;
            mode_fading_out = false;
            mode_fading_in = true;
            mode_fade_timer = 0.0f;
        }
        else
        {
            mode_fading_in = false;
        }
    }

    /** @brief Draw the current mode-transition fade overlay; call right before presenting a frame. */
    void apply_mode_fade()
    {
        if (mode_fade_alpha == 0)
        {
            return;
        }
        mode_fade.set_colour(simlib::Colour{0, 0, 0, mode_fade_alpha});
        mode_fade.apply();
    }

    /** @brief Start/resume/pause the music stream so it only plays while in Mode::playing. */
    void sync_background_music()
    {
        if (!background_music)
        {
            return;
        }
        const bool want_active = current_mode == Mode::playing && !background_music_paused;
        if (want_active && !background_music_active)
        {
            if (!background_music_started)
            {
                simlib::music_set_volume(20);
                simlib::play_stream(background_music);
                background_music_started = true;
            }
            else
            {
                simlib::resume_stream();
            }
            background_music_active = true;
        }
        else if (!want_active && background_music_active)
        {
            simlib::pause_stream();
            background_music_active = false;
        }
    }

    /** @brief Release game-owned resources before their dependent simlib subsystems. */
    void shutdown()
    {
        shutdown_lua_console();
        shutdown_playing();
        simlib::destroy_bitmap(playing_background);
        simlib::destroy_bitmap(dirt_texture);
        simlib::destroy_bitmap(grass_texture);
        for (simlib::Bitmap* texture : balloon_textures)
        {
            simlib::destroy_bitmap(texture);
        }
        balloon_textures.clear();
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
        simlib::set_window_title("Balloons!");
        current_mode = Mode::menu;
        simlib::gui_init();
        if (!simlib::gamepad_init())
        {
            return false;
        }
        simlib::set_fps(60);
        simlib::music_init();
        background_music = simlib::load_stream("assets/music/Solar Serenity.ogg");
        simlib::audio_fx_init();
        thrust_sound = simlib::load_sample("assets/sfx/sustain.wav");
        burner_sound = simlib::load_sample("assets/sfx/engines.wav");
        SDL_StopTextInput();

        balloon_textures = {
            simlib::load_bitmap("assets/textures/balloon_red.png"),
            simlib::load_bitmap("assets/textures/balloon_blue.png"),
            simlib::load_bitmap("assets/textures/balloon_green.png"),
        };
        playing_background = simlib::load_bitmap("assets/textures/Bumpy_Sky-Blue_01-512x512.png");
        dirt_texture = simlib::load_bitmap("assets/textures/dirt.png");
        grass_texture = simlib::load_bitmap("assets/textures/grass.png");
        return true;
    }

    /** @brief Select the current mode's complete update-and-render operation. */
    void update_and_render()
    {
        update_mode_fade();
        sync_background_music();
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
