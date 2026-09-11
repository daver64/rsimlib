#include "sl.h"

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 500))
    {
        return -1;
    }

    sl::Bitmap *target = sl::create_render_target(320, 220);
    if (!target)
    {
        sl::shutdown();
        return -1;
    }

    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }

        if (sl::begin_render_target(target))
        {
            sl::clear_render_target({25, 33, 48});
            sl::rectfill(target, 20, 20, 300, 200, {62, 111, 168});
            sl::circlefill(target, 160, 110, 62, {236, 170, 76});
            sl::trianglefill(target, 80, 165, 160, 45, 240, 165, {222, 92, 88});
            sl::end_render_target();
        }

        sl::clear_to_colour(sl::screen, {14, 18, 25});
        sl::draw_sprite_v_flip(target, 240, 130);
        sl::gprintf_center(32, {232, 236, 244}, "Render target example - press Escape to exit");
        sl::show_video_bitmap();
        sl::end_frame();
    }
    sl::wait_for_graphics();
    sl::destroy_bitmap(target);
    sl::shutdown();
    return 0;
}