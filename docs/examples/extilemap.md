# extilemap

Tile map rendering: a map built in code, or a Tiled `.tmj` map loaded from a path, scrolled with the arrow keys with tile collision.

![extilemap screenshot](img/extilemap.png)

## What it shows
- Building a map with `create_tilemap`, `tilemap_add_tileset`, `tilemap_add_layer` and `tilemap_set`
- Loading a Tiled JSON map with `load_tiled_map`
- Solid tiles and collision queries with `tilemap_set_solid` and `tilemap_rect_blocked`
- Flipped tiles and animated tile updates with `update_tilemap`

## Key code

In `extilemap.cpp`, the map is built with `create_tilemap`, `tilemap_add_tileset`, `tilemap_add_layer` and `tilemap_set`, using a flip flag on some tiles, and can instead come from `load_tiled_map`.

```cpp
        map = sl::load_tiled_map(map_path);
// ...
        map = atlas.tile_count > 0 ? sl::create_tilemap(60, 40, 32, 32) : nullptr;
        if (map)
        {
            sl::tilemap_add_tileset(map, atlas);
            // Atlas tiles are named by their (column, row) in the 16x16 sheet; ids are 1-based.
            const auto tile = [](int x, int y) { return y * 16 + x + 1; };
            const int grass = tile(6, 6), road = tile(11, 1), granite = tile(6, 9);
            const int ground = sl::tilemap_add_layer(map, "ground");
            const int walls = sl::tilemap_add_layer(map, "walls");
// ...
                    sl::tilemap_set(map, ground, column, row, cell);
// ...
                        sl::tilemap_set(map, walls, column, row, granite | (pillar ? sl::tile_flip_horizontal : 0));
```

Solid tiles and an animated tile are set up with `tilemap_set_solid` and a tile animation.

```cpp
            sl::tilemap_set_solid(map, granite, true);
            // Swap the two water tiles back and forth to show tile animation.
            auto &tiles = map->tilesets[0].tiles;
            tiles[water - 1].animation = {{water - 1, 400}, {water_alt - 1, 400}};
```

Each frame, `update_tilemap` advances the animation and `tilemap_rect_blocked` resolves each movement axis separately.

```cpp
        sl::update_tilemap(map, dt);
// ...
        if (!collide || !sl::tilemap_rect_blocked(map, player_x + move_x, player_y, player_size, player_size))
            player_x += move_x;
        if (!collide || !sl::tilemap_rect_blocked(map, player_x, player_y + move_y, player_size, player_size))
            player_y += move_y;
```

## Running
```bash
./build/extilemap [map.tmj]
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Without a path the example builds its own map. Pass a Tiled `.tmj` file to view it.

## Controls
| Input | Action |
|-------|--------|
| Arrow keys | Move |
| Escape | Quit |

## Key APIs
- `create_tilemap`, `tilemap_add_tileset`, `tilemap_add_layer`, `tilemap_set`, `load_tiled_map` — [Tile Maps](../api_reference.md#tile-maps-includetilemaph) · [`tilemap.h`](../../include/tilemap.h)
- `tilemap_set_solid`, `tilemap_rect_blocked`, `draw_tilemap`, `update_tilemap` — [Tile Maps](../api_reference.md#tile-maps-includetilemaph) · [`tilemap.h`](../../include/tilemap.h)
- `create_atlas` — [Atlases](../api_reference.md#atlases--animated-sprites-includeatlash-includeanimated_spriteh) · [`atlas.h`](../../include/atlas.h)

## Source
[src/examples/extilemap/extilemap.cpp](../../src/examples/extilemap/extilemap.cpp)

## See also
- [exisometric](exisometric.md)
- [ex2dfeatures](ex2dfeatures.md)
- [exatlas](exatlas.md)
