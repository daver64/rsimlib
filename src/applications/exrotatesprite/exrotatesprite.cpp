#include "sl.h"

#include <cmath>

int main(int argc, char *argv[])
{
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Bitmap *balloon = sl::load_bitmap("assets/textures/balloon_red.png");
    if (!balloon)
    {
        sl::shutdown();
        return -1;
    }

    sl::set_fps(60);
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
        }

        const float angle_degrees = static_cast<float>(sl::time_ms() % 3600) * 0.1f;
        sl::clear_to_colour(sl::screen, {45, 48, 56});
        sl::gprintf_center(32, {0, 255, 0}, "Rotating sprite - press Escape to exit");
        sl::draw_sprite_rotated(balloon, 400.0f, 300.0f, angle_degrees);
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::destroy_bitmap(balloon);
    sl::shutdown();
    return 0;
}
