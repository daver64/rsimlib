#include "sl.h"

#include <algorithm>
#include <cstdio>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 900, 600))
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

    // Atlas display positioning on the right
    const int atlas_x = 364;
    const int atlas_y = 44;
    const int atlas_w = 512;
    const int atlas_h = 512;

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
                else if (event.key() == sl::Event::Key::arrow_up)
                    selected_tile = (selected_tile + atlas.tile_count - atlas.columns) % atlas.tile_count;
                else if (event.key() == sl::Event::Key::arrow_down)
                    selected_tile = (selected_tile + atlas.columns) % atlas.tile_count;
            }
            else if (event.type() == sl::Event::Type::mouse_button_down && event.mouse_button() == 1)
            {
                const int mx = sl::mouse_x();
                const int my = sl::mouse_y();
                if (mx >= atlas_x && mx < atlas_x + atlas_w &&
                    my >= atlas_y && my < atlas_y + atlas_h)
                {
                    const int col = (mx - atlas_x) / atlas.tile_width;
                    const int row = (my - atlas_y) / atlas.tile_height;
                    if (col >= 0 && col < atlas.columns && row >= 0 && row < atlas.rows)
                    {
                        const int index = row * atlas.columns + col;
                        if (index >= 0 && index < atlas.tile_count)
                        {
                            selected_tile = index;
                        }
                    }
                }
            }
            sl::display_handle_event(event);
        }

        // Also allow click-and-drag selection
        if (sl::mouse_buttons() & 1)
        {
            const int mx = sl::mouse_x();
            const int my = sl::mouse_y();
            if (mx >= atlas_x && mx < atlas_x + atlas_w &&
                my >= atlas_y && my < atlas_y + atlas_h)
            {
                const int col = (mx - atlas_x) / atlas.tile_width;
                const int row = (my - atlas_y) / atlas.tile_height;
                if (col >= 0 && col < atlas.columns && row >= 0 && row < atlas.rows)
                {
                    const int index = row * atlas.columns + col;
                    if (index >= 0 && index < atlas.tile_count)
                    {
                        selected_tile = index;
                    }
                }
            }
        }

        sl::clear_to_colour(sl::screen, {24, 28, 38});

        // --- Left Panel: Text Information and Selected Tile Preview ---
        const sl::Colour heading{235, 220, 155};
        const sl::Colour text{218, 226, 235};
        const sl::Colour muted{145, 160, 178};
        const sl::Colour highlight{255, 215, 92};

        sl::gprintf(32, 44, heading, "Texture Atlas Example");
        sl::gprintf(32, 74, muted, "Arrows / Mouse Click: select tile");
        sl::gprintf(32, 98, muted, "Escape: exit");

        // Selected tile info
        const int sel_col = selected_tile % atlas.columns;
        const int sel_row = selected_tile / atlas.columns;
        sl::gprintf(32, 144, text, "Selected Tile: %d / %d", selected_tile + 1, atlas.tile_count);
        sl::gprintf(32, 168, muted, "Grid Position: col %d, row %d", sel_col + 1, sel_row + 1);
        sl::gprintf(32, 192, muted, "Atlas Size:    %d x %d (%d cols x %d rows)",
                    bitmap->width, bitmap->height, atlas.columns, atlas.rows);
        sl::gprintf(32, 216, muted, "Tile Size:     %d x %d px", atlas.tile_width, atlas.tile_height);

        // Preview boxes on the left
        sl::gprintf(32, 268, heading, "1x (32x32)");
        sl::rectfill(sl::screen, 32.0f, 294.0f, 65.0f, 327.0f, {15, 18, 25});
        sl::rect(sl::screen, 31.0f, 293.0f, 66.0f, 328.0f, {60, 75, 95});
        sl::atlas_blit(atlas, sl::screen, selected_tile, 33, 295);

        sl::gprintf(120, 268, heading, "4x Preview (128x128)");
        sl::rectfill(sl::screen, 120.0f, 294.0f, 250.0f, 424.0f, {15, 18, 25});
        sl::rect(sl::screen, 119.0f, 293.0f, 251.0f, 425.0f, highlight, 2.0f);
        sl::atlas_stretch_blit(atlas, sl::screen, selected_tile, 121, 295, 128, 128);

        // --- Right Panel: Entire Atlas Bitmap with Selection Grid ---
        sl::gprintf(atlas_x, 24, heading, "Full Atlas Texture (%d x %d)", bitmap->width, bitmap->height);

        // Background / border for atlas
        sl::rectfill(sl::screen, static_cast<float>(atlas_x - 2), static_cast<float>(atlas_y - 2),
                     static_cast<float>(atlas_x + atlas_w + 1), static_cast<float>(atlas_y + atlas_h + 1), {15, 18, 25});
        sl::rect(sl::screen, static_cast<float>(atlas_x - 2), static_cast<float>(atlas_y - 2),
                 static_cast<float>(atlas_x + atlas_w + 1), static_cast<float>(atlas_y + atlas_h + 1), {60, 75, 95});

        // Draw full atlas bitmap
        sl::draw_sprite(bitmap, static_cast<float>(atlas_x), static_cast<float>(atlas_y));

        // Draw selection rectangle over the selected tile in the full atlas
        const float sel_x = static_cast<float>(atlas_x + sel_col * atlas.tile_width);
        const float sel_y = static_cast<float>(atlas_y + sel_row * atlas.tile_height);
        sl::rect(sl::screen, sel_x, sel_y,
                 sel_x + static_cast<float>(atlas.tile_width - 1),
                 sel_y + static_cast<float>(atlas.tile_height - 1),
                 highlight, 2.0f);

        // Hover highlight
        const int mx = sl::mouse_x();
        const int my = sl::mouse_y();
        if (mx >= atlas_x && mx < atlas_x + atlas_w &&
            my >= atlas_y && my < atlas_y + atlas_h)
        {
            const int hover_col = (mx - atlas_x) / atlas.tile_width;
            const int hover_row = (my - atlas_y) / atlas.tile_height;
            if (hover_col != sel_col || hover_row != sel_row)
            {
                const float hov_x = static_cast<float>(atlas_x + hover_col * atlas.tile_width);
                const float hov_y = static_cast<float>(atlas_y + hover_row * atlas.tile_height);
                sl::rect(sl::screen, hov_x, hov_y,
                         hov_x + static_cast<float>(atlas.tile_width - 1),
                         hov_y + static_cast<float>(atlas.tile_height - 1),
                         {255, 255, 255, 180}, 1.0f);
            }
        }

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_bitmap(bitmap);
    sl::shutdown();
    return 0;
}
