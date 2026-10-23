#include "sl.h"

#include <algorithm>

// Scrolling tile map with collision and an animated tile. Pass a Tiled .tmj/.json/.tmx path to view your own map,
// otherwise a map is generated from assets/textures/atlas_grid.png.
int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    std::string map_path;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument.rfind("--", 0) != 0)
            map_path = argument;
    }

    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 600))
    {
        return -1;
    }

    sl::TileMap *map = nullptr;
    sl::Bitmap *atlas_bitmap = nullptr;
    if (!map_path.empty())
    {
        map = sl::load_tiled_map(map_path);
    }
    else
    {
        atlas_bitmap = sl::load_bitmap("assets/textures/atlas_grid.png");
        const sl::Atlas atlas = sl::create_atlas(atlas_bitmap, 32, 32);
        map = atlas.tile_count > 0 ? sl::create_tilemap(60, 40, 32, 32) : nullptr;
        if (map)
        {
            sl::tilemap_add_tileset(map, atlas);
            // Atlas tiles are named by their (column, row) in the 16x16 sheet; ids are 1-based.
            const auto tile = [](int x, int y) { return y * 16 + x + 1; };
            const int grass = tile(6, 6), road = tile(11, 1), granite = tile(6, 9);
            const int water = tile(0, 10), water_alt = tile(12, 11);
            const int ground = sl::tilemap_add_layer(map, "ground");
            const int walls = sl::tilemap_add_layer(map, "walls");
            for (int row = 0; row < map->height; ++row)
            {
                for (int column = 0; column < map->width; ++column)
                {
                    const bool pond = column % 14 >= 6 && column % 14 <= 9 && row % 10 >= 5 && row % 10 <= 7;
                    const bool path = row == 2 || column == 2;
                    std::uint32_t cell = grass | ((column + row) % 3 == 0 ? sl::tile_flip_horizontal : 0);
                    if (pond)
                        cell = water;
                    else if (path)
                        cell = road;
                    sl::tilemap_set(map, ground, column, row, cell);
                    const bool border = column == 0 || row == 0 || column == map->width - 1 || row == map->height - 1;
                    const bool pillar = column % 7 == 3 && row % 5 == 2 && !pond;
                    if (border || pillar)
                        sl::tilemap_set(map, walls, column, row, granite | (pillar ? sl::tile_flip_horizontal : 0));
                }
            }
            sl::tilemap_set_solid(map, granite, true);
            // Swap the two water tiles back and forth to show tile animation.
            auto &tiles = map->tilesets[0].tiles;
            tiles[water - 1].animation = {{water - 1, 400}, {water_alt - 1, 400}};
        }
    }
    if (!map)
    {
        sl::destroy_bitmap(atlas_bitmap);
        sl::shutdown();
        return -1;
    }

    const float map_width = static_cast<float>(map->width * map->tile_width);
    const float map_height = static_cast<float>(map->height * map->tile_height);
    float player_x = static_cast<float>(map->tile_width) * 2.0f;
    float player_y = static_cast<float>(map->tile_height) * 2.0f;
    const float player_size = 20.0f;
    const bool collide = map->orientation == sl::TileMapOrientation::orthogonal;

    sl::set_fps(60);
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }

        const float dt = static_cast<float>(sl::get_frame_time()) / 1000.0f;
        sl::update_tilemap(map, dt);

        const float step = 180.0f * (dt > 0.0f ? dt : 1.0f / 60.0f);
        float move_x = 0.0f, move_y = 0.0f;
        if (sl::key_down(sl::Event::Key::arrow_left))
            move_x -= step;
        if (sl::key_down(sl::Event::Key::arrow_right))
            move_x += step;
        if (sl::key_down(sl::Event::Key::arrow_up))
            move_y -= step;
        if (sl::key_down(sl::Event::Key::arrow_down))
            move_y += step;
        // Resolve each axis separately so the player slides along walls.
        if (!collide || !sl::tilemap_rect_blocked(map, player_x + move_x, player_y, player_size, player_size))
            player_x += move_x;
        if (!collide || !sl::tilemap_rect_blocked(map, player_x, player_y + move_y, player_size, player_size))
            player_y += move_y;

        const float view_w = static_cast<float>(sl::screen_width());
        const float view_h = static_cast<float>(sl::screen_height());
        const float camera_x = std::clamp(player_x + player_size * 0.5f - view_w * 0.5f, 0.0f, std::max(map_width - view_w, 0.0f));
        const float camera_y = std::clamp(player_y + player_size * 0.5f - view_h * 0.5f, 0.0f, std::max(map_height - view_h, 0.0f));

        sl::clear_to_colour(sl::screen, {20, 24, 32});
        sl::draw_tilemap(map, camera_x, camera_y);
        sl::rectfill(sl::screen, player_x - camera_x, player_y - camera_y,
                     player_x - camera_x + player_size, player_y - camera_y + player_size, {255, 215, 92});
        sl::gprintf(12, 12, {235, 235, 235}, "Arrows: move   Esc: quit   (%s)",
                    map_path.empty() ? "generated map" : map_path.c_str());
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_tilemap(map);
    sl::destroy_bitmap(atlas_bitmap);
    sl::shutdown();
    return 0;
}
