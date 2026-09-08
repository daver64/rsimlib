#include "sl.h"
#include <iostream>
void process_input()
{
    bool running=true;
    while (running)
    {
        SDL_Event event;
        
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_KEYDOWN || event.type==SDL_QUIT)
            {
                running = false;
            }   // Handle input here
        }
    }
}
int main(int argc, char *argv[])
{
    if (!simlib::set_gfx_mode(simlib::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    simlib::Bitmap *bitmap = simlib::load_bitmap("assets/textures/balloon_red.png");
    if(!bitmap)
    {
        std::cerr << "Failed to load bitmap!" << std::endl;
        return -2;
    }
    simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});
    simlib::blit(bitmap, simlib::screen, 0, 0, 100, 100, bitmap->width, bitmap->height);


    simlib::show_video_bitmap();
    simlib::end_frame();
    process_input();
    simlib::shutdown();
    return 0;
}
