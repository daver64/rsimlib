#include "sl.h"

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Event event;
    bool running = true;

    while (running)
    {
        
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;

            if(event.type() == sl::Event::Type::key_up)
            {
                if(event.key() == sl::Event::Key::escape)
                    running = false;
            }
            sl::display_handle_event(event);            
        }
        sl::clear_to_colour(sl::screen, sl::OrangeRed);
        // ... draw your frame ...
        sl::show_video_bitmap();
        sl::end_frame();
    }
    sl::wait_for_graphics();
    sl::shutdown();
}