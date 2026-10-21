# extilemap

Tile map rendering: a map built in code, or a Tiled `.tmj` map loaded from a path, scrolled with the arrow keys with tile collision.

![extilemap screenshot](img/extilemap.png)

## What it shows
- Building a map with `create_tilemap`, `tilemap_add_tileset`, `tilemap_add_layer` and `tilemap_set`
- Loading a Tiled JSON map with `load_tiled_map`
- Solid tiles and collision queries with `tilemap_set_solid` and `tilemap_rect_blocked`
- Flipped tiles and animated tile updates with `update_tilemap`

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
