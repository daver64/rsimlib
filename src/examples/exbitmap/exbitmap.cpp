#include "sl.h"
#include <iostream>
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

    sl::Bitmap *bitmap = sl::load_bitmap("assets/textures/balloon_red.png");
    if(!bitmap)
    {
        std::cerr << "Failed to load bitmap!" << std::endl;
        return -2;
    }
    sl::clear_to_colour(sl::screen, sl::Colour{45, 48, 56});
    sl::blit(bitmap, sl::screen, 0, 0, 100, 100, bitmap->width, bitmap->height);


    sl::show_video_bitmap();
    sl::end_frame();
    process_input();
    sl::shutdown();
    return 0;
}
