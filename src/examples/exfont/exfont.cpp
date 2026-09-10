#include "sl.h"

int main(int argc, char* argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Font* sans_font = sl::open_sans_font(28);
    sl::Font* monospace_font = sl::get_default_monospace_font();
    sl::clear_to_colour(sl::screen, {45, 48, 56});

    if (sans_font)
    {
        const std::string text = "Proportional TrueType: wide W, narrow i";
        const int x = (sl::screen->width - sl::text_length(sans_font, text)) / 2;
        sl::textout(sans_font, x, 150, {255, 220, 90}, text);
    }
    if (monospace_font)
    {
        const std::string text = "Monospace TrueType: wide W, narrow i";
        const int x = (sl::screen->width - sl::text_length(monospace_font, text)) / 2;
        sl::textout(monospace_font, x, 250, {110, 220, 255}, text);
    }

    sl::show_video_bitmap();
    sl::end_frame();

    sl::Event event;
    bool running = true;
    while (running)
    {
        while (sl::poll_event(&event))
        {
            running = event.type() != sl::Event::Type::quit && event.type() != sl::Event::Type::key_down;
        }
    }

    if (sans_font)
    {
        sl::close_font(sans_font);
    }
    sl::shutdown();
    return 0;
}