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

    simlib::gprintf_center(32, text_colour, "Triangle drawing examples");

    simlib::triangle(
        simlib::screen, 100.0f, 210.0f, 220.0f, 80.0f, 340.0f, 210.0f,
        simlib::Colour{255, 210, 60});
    simlib::trianglefill(
        simlib::screen, 460.0f, 210.0f, 580.0f, 80.0f, 700.0f, 210.0f,
        simlib::Colour{60, 180, 255});

    simlib::Bitmap *texture = simlib::load_bitmap("assets/textures/balloon_red.png");
    if (texture)
    {
        simlib::triangle(
            simlib::screen, 100.0f, 480.0f, 220.0f, 350.0f, 340.0f, 480.0f,
            texture);
        simlib::trianglefill(
            simlib::screen, 460.0f, 480.0f, 580.0f, 350.0f, 700.0f, 480.0f,
            texture);
    }

    simlib::show_video_bitmap();
    simlib::end_frame();
    process_input();
    simlib::destroy_bitmap(texture);
    simlib::shutdown();
    return 0;
}
