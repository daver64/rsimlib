#include "game.h"


namespace game
{
    std::atomic<bool> running{true};

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
        simlib::gui_init();
        simlib::set_fps(60);
        return true;
    }

    void update_and_render()
    {
        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});

        const double frame_time = simlib::get_frame_time();
        const int x = 1;
        const int y = 1;
        gprintf(x, y+fontheight,
                text_colour, "frame time: %.2f ms (%.1f fps)",
                frame_time, frame_time > 0.0 ? 1000.0 / frame_time : 0.0);
        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}
