#include "sl.h"

int main()
{
    if (!sl::set_graphics_backend(sl::GraphicsBackend::vulkan) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
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
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, {24, 30, 42});
        sl::gprintf_center(32, {220, 230, 240}, "Vulkan textured quad smoke test - Escape to exit");
        sl::draw_sprite_rotated(balloon, 400.0f, 300.0f, static_cast<float>(sl::time_ms() % 3600) * 0.1f);
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::destroy_bitmap(balloon);
    sl::shutdown();
    return 0;
}
