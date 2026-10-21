# exatlas

Texture atlas browser: pick a tile from a grid atlas and see it drawn at 4x next to the full atlas texture.

![exatlas screenshot](img/exatlas.png)

## What it shows
- Building an atlas from a bitmap with `create_atlas`
- Drawing individual tiles with `atlas_blit` and scaled with `atlas_stretch_blit`
- Mapping mouse position to a grid cell
- Showing tile index, grid position, atlas size and tile size as live text

## Key code

In `exatlas.cpp`, the atlas is built from a loaded bitmap by slicing it into 32x32 tiles with `create_atlas`.

```cpp
sl::Bitmap *bitmap = sl::load_bitmap("assets/textures/atlas_grid.png");
const sl::Atlas atlas = sl::create_atlas(bitmap, 32, 32);
```

The selected tile is drawn at 1x with `atlas_blit` and scaled to 128x128 with `atlas_stretch_blit`.

```cpp
sl::atlas_blit(atlas, sl::screen, selected_tile, 33, 295);
// ...
sl::atlas_stretch_blit(atlas, sl::screen, selected_tile, 121, 295, 128, 128);
```

Mouse clicks are mapped to a grid cell by dividing the mouse offset by the tile size, and the live text shows the tile index, grid position and sizes.

```cpp
const int col = (mx - atlas_x) / atlas.tile_width;
const int row = (my - atlas_y) / atlas.tile_height;
if (col >= 0 && col < atlas.columns && row >= 0 && row < atlas.rows)
{
    const int index = row * atlas.columns + col;
    // ...
}
// ...
sl::gprintf(32, 144, text, "Selected Tile: %d / %d", selected_tile + 1, atlas.tile_count);
sl::gprintf(32, 192, muted, "Atlas Size:    %d x %d (%d cols x %d rows)",
            bitmap->width, bitmap->height, atlas.columns, atlas.rows);
```

## Running
```bash
./build/exatlas
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Loads its atlas image from `assets/textures/`, so run from the repository root.

## Controls
| Input | Action |
|-------|--------|
| Arrow keys | Move the selected tile |
| Mouse click | Select the tile under the cursor |
| Escape | Exit |

## Key APIs
- `create_atlas`, `atlas_blit`, `atlas_stretch_blit`, `draw_sprite` — [Atlases](../api_reference.md#atlases--animated-sprites-includeatlash-includeanimated_spriteh) · [`atlas.h`](../../include/atlas.h)
- `mouse_x`, `mouse_y`, `mouse_buttons` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)
- `rect`, `rectfill`, `gprintf` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/exatlas/exatlas.cpp](../../src/examples/exatlas/exatlas.cpp)

## See also
- [exanimsprite](exanimsprite.md)
- [extilemap](extilemap.md)
- [exbitmap](exbitmap.md)
