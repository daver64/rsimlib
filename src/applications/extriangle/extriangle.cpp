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

    sl::gprintf_center(32, text_colour, "Triangle drawing examples");

    sl::triangle(
        sl::screen, 100.0f, 210.0f, 220.0f, 80.0f, 340.0f, 210.0f,
        sl::Colour{255, 210, 60});
    sl::trianglefill(
        sl::screen, 460.0f, 210.0f, 580.0f, 80.0f, 700.0f, 210.0f,
        sl::Colour{60, 180, 255});

    sl::Bitmap *texture = sl::load_bitmap("assets/textures/balloon_red.png");
    if (texture)
    {
        sl::triangle(
            sl::screen, 100.0f, 480.0f, 220.0f, 350.0f, 340.0f, 480.0f,
            texture);
        sl::trianglefill(
            sl::screen, 460.0f, 480.0f, 580.0f, 350.0f, 700.0f, 480.0f,
            texture);
    }

    sl::show_video_bitmap();
    sl::end_frame();
    process_input();
    sl::destroy_bitmap(texture);
    sl::shutdown();
    return 0;
}
