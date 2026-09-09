#include "sl.h"

int main()
{
    if (!simlib::set_gfx_mode(simlib::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    simlib::Event event;
    bool running = true;
    while (running)
    {
        while (simlib::poll_event(&event))
        {
            if (event.type() == simlib::Event::Type::quit)
                running = false;
            simlib::display_handle_event(event);
        }

        simlib::clear_to_colour(simlib::screen, simlib::Colour{146, 200, 62});
        // ... draw your frame ...
        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}