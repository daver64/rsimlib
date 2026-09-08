#include "sl.h"

int main(int argc, char* argv[])
{
    if (!simlib::set_gfx_mode(simlib::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    simlib::Font* sans_font = simlib::open_sans_font(28);
    simlib::Font* monospace_font = simlib::get_default_monospace_font();
    simlib::clear_to_colour(simlib::screen, {45, 48, 56});

    if (sans_font)
    {
        const std::string text = "Proportional TrueType: wide W, narrow i";
        const int x = (simlib::screen->width - simlib::text_length(sans_font, text)) / 2;
        simlib::textout(sans_font, x, 150, {255, 220, 90}, text);
    }
    if (monospace_font)
    {
        const std::string text = "Monospace TrueType: wide W, narrow i";
        const int x = (simlib::screen->width - simlib::text_length(monospace_font, text)) / 2;
        simlib::textout(monospace_font, x, 250, {110, 220, 255}, text);
    }

    simlib::show_video_bitmap();
    simlib::end_frame();

    simlib::Event event;
    bool running = true;
    while (running)
    {
        while (simlib::poll_event(&event))
        {
            running = event.type() != simlib::Event::Type::quit && event.type() != simlib::Event::Type::key_down;
        }
    }

    if (sans_font)
    {
        simlib::close_font(sans_font);
    }
    simlib::shutdown();
    return 0;
}