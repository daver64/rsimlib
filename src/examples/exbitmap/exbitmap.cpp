#include "sl.h"
#include <iostream>
int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Bitmap *bitmap = sl::load_bitmap("assets/textures/balloon_red.png");
    if (!bitmap)
    {
        std::cerr << "Failed to load bitmap!" << std::endl;
        return -2;
    }
    sl::set_fps(60);
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::key_down || event.type() == sl::Event::Type::quit)
            {
                running = false;
            }
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, sl::Colour{45, 48, 56});
        sl::blit(bitmap, sl::screen, 0, 0, 100, 100, bitmap->width, bitmap->height);
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }
    sl::wait_for_graphics();
    sl::destroy_bitmap(bitmap);
    sl::shutdown();
    return 0;
}
