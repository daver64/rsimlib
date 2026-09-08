#include "sl.h"

void process_input()
{
    bool running=true;
    while (running)
    {
        simlib::Event event;
        
        while (simlib::poll_event(&event))
        {
            if (event.type() == simlib::Event::Type::key_down || event.type() == simlib::Event::Type::quit)
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

    simlib::Font *font = simlib::get_default_monospace_font();
    const int fontheight = simlib::text_height(font);
    simlib::Colour text_colour{0, 255, 0};
    simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});

    simlib::gprintf_center(1 + fontheight, text_colour, "Hello, world!");
    simlib::show_video_bitmap();
    simlib::end_frame();
    process_input();
    simlib::shutdown();
    return 0;
}
