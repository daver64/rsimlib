#include "sl.h"

void process_input()
{
    bool running=true;
    while (running)
    {
        sl::Event event;
        
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::key_down || event.type() == sl::Event::Type::quit)
            {
                running = false;
            }   // Handle input here
        }
    }
}

int main(int argc, char *argv[])
{

    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    
    sl::LuaCanvas canvas;

    const sl::LuaScriptResult result = canvas.run_file("assets/scripts/scene.lua");
    if (!result.success)
    {
        // Handle result.error
    }


    canvas.render(sl::screen);
    sl::show_video_bitmap();
    process_input();
    sl::wait_for_graphics();
    sl::shutdown();
}