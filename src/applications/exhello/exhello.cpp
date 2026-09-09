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
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Font *font = sl::get_default_monospace_font();
    const int fontheight = sl::text_height(font);
    sl::Colour text_colour{0, 255, 0};
    sl::clear_to_colour(sl::screen, sl::Colour{45, 48, 56});

    sl::gprintf_center(1 + fontheight, text_colour, "Hello, world!");
    sl::show_video_bitmap();
    sl::end_frame();
    process_input();
    sl::shutdown();
    return 0;
}
