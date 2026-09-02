#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "audio_test.h"
#include "display.h"
#include "draw.h"
#include "font.h"
#include "graphics_fx_test.h"

#include <iostream>

int main(int argc, char* argv[]) {
    if (!rvoid::display::set_gfx_mode(rvoid::display::GFX_AUTODETECT_WINDOWED, 800, 600)) {
        return -1;
    }

    if (TTF_Init() != 0) {
        rvoid::display::shutdown();
        return -1;
    }

    TTF_Font* font = open_monospace_font(22);

    if (!font) {
        std::cerr << "Failed to create font from loaded bytes.\n";
    }

    //audio_test::initialize();
    //graphics_fx_test::initialise();

    // Main loop
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            //audio_test::handle_event(event);
            rvoid::display::handle_event(event);
        }

        rvoid::draw::clear_to_colour(rvoid::draw::screen, rvoid::draw::Colour{45, 48, 56});
        //graphics_fx_test::render();
        rvoid::display::show_video_bitmap();
    }

    // Clean up
    if (font) {
        TTF_CloseFont(font);
    }
    audio_test::shutdown();
    graphics_fx_test::shutdown();
    TTF_Quit();
    rvoid::display::shutdown();

    return 0;
}
