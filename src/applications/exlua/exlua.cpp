#include "sl.h"

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

int main()
{

    if (!simlib::set_gfx_mode(simlib::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    
    simlib::LuaCanvas canvas;

    const simlib::LuaScriptResult result = canvas.run_file("assets/scripts/scene.lua");
    if (!result.success)
    {
        // Handle result.error
    }


    canvas.render(simlib::screen);
    simlib::show_video_bitmap();
    process_input();
    return 0;
}