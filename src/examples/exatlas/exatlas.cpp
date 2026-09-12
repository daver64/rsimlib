#include "sl.h"

#include <cstdio>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Bitmap *bitmap = sl::load_bitmap("assets/textures/atlas_grid.png");
    const sl::Atlas atlas = sl::create_atlas(bitmap, 32, 32);
    if (!bitmap || atlas.tile_count <= 0)
    {
        sl::destroy_bitmap(bitmap);
        sl::shutdown();
        return -1;
    }

    int selected_tile = 0;
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            else if (event.type() == sl::Event::Type::key_down && !event.key_repeat())
            {
                if (event.key() == sl::Event::Key::arrow_left)
                    selected_tile = (selected_tile + atlas.tile_count - 1) % atlas.tile_count;
                else if (event.key() == sl::Event::Key::arrow_right)
                    selected_tile = (selected_tile + 1) % atlas.tile_count;
            }
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, {24, 28, 38});
        sl::gprintf_center(48, {232, 236, 244}, "Texture atlas example");
        sl::gprintf_center(78, {170, 185, 205}, "Left/Right: select sprite    Escape: exit");
        sl::atlas_stretch_blit(atlas, sl::screen, selected_tile, 384, 284, 32, 32);
        sl::gprintf_center(356, {170, 185, 205}, "Sprite %d / %d", selected_tile + 1, atlas.tile_count);
        sl::gprintf_center(390, {135, 150, 170}, "Atlas: %d columns x %d rows", atlas.columns, atlas.rows);
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_bitmap(bitmap);
    sl::shutdown();
    return 0;
}
