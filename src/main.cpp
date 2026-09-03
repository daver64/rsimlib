#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "audio.h"
#include "display.h"
#include "draw.h"
#include "font.h"
#include "graphics_fx_test.h"
#include "gui.h"
#include "system.h"

#include <imgui.h>

/** Initialize subsystems and run the SDL event/render loop. */
int main(int argc, char *argv[])
{
    if (!simlib::display::set_gfx_mode(simlib::display::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    simlib::gui::init();

    TTF_Font *font = simlib::display::get_default_monospace_font();

    simlib::system::set_fps(0);

    // Main loop
    bool running = true;
    while (running)
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
            // audio_test::handle_event(event);
            simlib::display::handle_event(event);
            simlib::gui::handle_event(event);
        }

        simlib::draw::clear_to_colour(simlib::draw::screen, simlib::draw::Colour{45, 48, 56});
        simlib::display::textout(font, 1, 1,
                                 simlib::draw::Colour{250, 0, 0}, "Hello, World!");
        const double frame_time = simlib::system::get_frame_time();
        simlib::display::textprintf(font, 1, 1 + simlib::display::text_height(font),
                                    simlib::draw::Colour{250, 0, 0}, "frame time: %.2f ms (%.1f fps)",
                                    frame_time, frame_time > 0.0 ? 1000.0 / frame_time : 0.0);

        simlib::display::show_video_bitmap();
        simlib::system::end_frame();
    }

    // Clean up
    simlib::gui::shutdown();
    simlib::display::shutdown();
    simlib::audio_fx::shutdown();
    return 0;
}
