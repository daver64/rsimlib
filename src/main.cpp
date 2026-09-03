#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdarg>
#include <cstdio>

#include "audio.h"
#include "display.h"
#include "draw.h"
#include "font.h"
#include "graphics_fx_test.h"
#include "gui.h"
#include "system.h"

#include <imgui.h>

namespace game
{
    bool running = true;

    void gprintf(int x, int y, simlib::draw::Colour colour, const char *fmt, ...)
    {
        TTF_Font *font = simlib::display::get_default_monospace_font();
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        simlib::display::textout(font, x, y, colour, buffer);
    }

    void shutdown_game()
    {
        simlib::gui::shutdown();
        simlib::display::shutdown();
        simlib::audio_fx::shutdown();
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
            simlib::display::handle_event(event);
            simlib::gui::handle_event(event);
        }
        return true;
    }

    bool initialise_game()
    {
        if (!simlib::display::set_gfx_mode(simlib::display::GFX_AUTODETECT_WINDOWED, 800, 600))
        {
            return false;
        }
        simlib::gui::init();
        simlib::system::set_fps(60);
        return true;
    }

    void update_and_render()
    {
        TTF_Font *font = simlib::display::get_default_monospace_font();
        simlib::draw::clear_to_colour(simlib::draw::screen, simlib::draw::Colour{45, 48, 56});

        const double frame_time = simlib::system::get_frame_time();
        gprintf(1, 1 + simlib::display::text_height(font),
                simlib::draw::Colour{250, 0, 0}, "frame time: %.2f ms (%.1f fps)",
                frame_time, frame_time > 0.0 ? 1000.0 / frame_time : 0.0);
        simlib::display::show_video_bitmap();
        simlib::system::end_frame();
    }
}

/** Initialize subsystems and run the SDL event/render loop. */
int main(int argc, char *argv[])
{
    if (!game::initialise_game())
    {
        return -1;
    }

    // Main loop
    while (game::running)
    {
        game::handle_events();
        game::update_and_render();
    }

    // Clean up
    game::shutdown_game();
    return 0;
}
