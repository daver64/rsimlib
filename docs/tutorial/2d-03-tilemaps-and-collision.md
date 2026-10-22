# Lesson 3 (2D): Tile maps and collision

[← Tutorial index](README.md) · Previous: [Sprites and animation](2d-02-sprites-and-animation.md)

**Goal:** build a scrolling world from tiles, and stop the player walking through walls.

## The idea

- A **tile map** is a grid where each cell holds a tile number from an atlas. It stores a big world cheaply and draws it fast.
- A map has one or more **layers** (ground, walls, decoration) drawn in order. A cell value of `0` means empty; tile numbers start at `1`, so cell value `1` is the first tile in the atlas.
- Mark a tile number as **solid** with `tilemap_set_solid`, then ask `tilemap_rect_blocked(map, x, y, w, h)` whether a rectangle overlaps any solid tile.
- `draw_tilemap(map, camera_x, camera_y)` draws the part of the map visible when the view's top-left is at `(camera_x, camera_y)` in world pixels. Moving the camera scrolls the world.
- Real projects usually design maps in the [Tiled](https://www.mapeditor.org/) editor and load them with `sl::load_tiled_map("level.tmx")` (JSON `.tmj` also works). Here we build one in code so there's nothing else to set up.

## The program

```cpp
#include "sl.h"

#include <algorithm>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    sl::set_fps(60);

    sl::Bitmap *sheet = sl::load_bitmap("assets/textures/atlas_grid.png");
    sl::TileMap *map = sl::create_tilemap(60, 40, 32, 32); // 60x40 tiles of 32x32 pixels
    if (!sheet || !map)
    {
        return -1;
    }
    sl::tilemap_add_tileset(map, sl::create_atlas(sheet, 32, 32));
    const int ground = sl::tilemap_add_layer(map, "ground");
    const int walls = sl::tilemap_add_layer(map, "walls");
    for (int row = 0; row < 40; ++row)
    {
        for (int column = 0; column < 60; ++column)
        {
            sl::tilemap_set(map, ground, column, row, 1 + ((column + row) % 2)); // checker floor
            const bool border = column == 0 || row == 0 || column == 59 || row == 39;
            const bool pillar = column % 8 == 4 && row % 6 == 3;
            if (border || pillar)
                sl::tilemap_set(map, walls, column, row, 20);
        }
    }
    sl::tilemap_set_solid(map, 20, true); // tile 20 blocks movement

    float x = 100.0f, y = 100.0f;
    const float size = 24.0f, speed = 200.0f;
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
        float move_x = 0.0f, move_y = 0.0f;
        if (sl::key_down(sl::Event::Key::arrow_left))
            move_x -= speed * dt;
        if (sl::key_down(sl::Event::Key::arrow_right))
            move_x += speed * dt;
        if (sl::key_down(sl::Event::Key::arrow_up))
            move_y -= speed * dt;
        if (sl::key_down(sl::Event::Key::arrow_down))
            move_y += speed * dt;

        // Test each axis on its own so the player slides along walls instead of sticking.
        if (!sl::tilemap_rect_blocked(map, x + move_x, y, size, size))
            x += move_x;
        if (!sl::tilemap_rect_blocked(map, x, y + move_y, size, size))
            y += move_y;

        const float camera_x = std::clamp(x - 400.0f, 0.0f, 60.0f * 32.0f - 800.0f);
        const float camera_y = std::clamp(y - 300.0f, 0.0f, 40.0f * 32.0f - 600.0f);

        sl::clear_to_colour(sl::screen, {20, 24, 32});
        sl::draw_tilemap(map, camera_x, camera_y);
        sl::rectfill(sl::screen, x - camera_x, y - camera_y, x - camera_x + size, y - camera_y + size, {255, 215, 92});
        sl::gprintf(10, 10, {255, 255, 255}, "Arrow keys walk. Walls are solid.");

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_tilemap(map);
    sl::destroy_bitmap(sheet);
    sl::shutdown();
    return 0;
}
```

## What just happened

- The map is 60×40 tiles, 1920×1280 pixels, bigger than the window. The camera is clamped so the view never leaves the map.
- The player is drawn at its world position minus the camera, which converts world coordinates to screen coordinates. Every scrolling game uses this subtraction.
- Moving X and Y separately is the simplest collision response that feels right: a diagonal push against a wall still slides along it.

## Try it

1. Make a second solid tile, say 21, and place it with a different pattern.
2. Animate a tile (water, torches): `map->tilesets[0].tiles[2].animation = {{2, 250}, {3, 250}, {4, 250}};` cycles tiles 2, 3 and 4 every 250 ms (tile ids here are 0-based within the tileset), and you call `sl::update_tilemap(map, dt)` each frame.
3. Save a small map from the Tiled editor as `.tmx` and load it with `sl::load_tiled_map`. Check `sl::last_error()` if it returns `nullptr`.

## Go further

- Examples: [`extilemap`](../examples/extilemap.md), [`exroguelike`](../examples/exroguelike.md). API: [Tile Maps](../api_reference.md#tile-maps-includetilemaph).
- Next: [Text and sound](2d-04-text-and-sound.md).
