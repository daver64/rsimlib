# ex2dfeatures

A tour of newer 2D features: text blocks, blend modes, clip rects, sprite animation clips, hex and infinite tile maps, and integer-scaled low-resolution targets.

![ex2dfeatures screenshot](img/ex2dfeatures.png)

## What it shows
- Word-wrapped, aligned, outlined and shadowed text with `draw_text_block` and `TextStyle`
- Blend modes with `set_blend_mode` and clip rectangles
- `Animator` clips from a `SpriteSheet` built from an atlas
- Hexagonal and infinite tile maps (`create_hex_tilemap`, `create_infinite_tilemap`)
- An 80x45 render target scaled with `compute_scale_fit` and `draw_scaled_target`
- Unicode text rendering

## Running
```bash
./build/ex2dfeatures
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Set `RSIMLIB_SCREENSHOT=<file.png>` to save a screenshot on an early frame.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- `draw_text_block`, `TextStyle`, `TextAlign` — [Typography](../api_reference.md#typography--text-rendering-includefonth) · [`font.h`](../../include/font.h)
- `set_blend_mode`, `set_clip_rect`, `create_render_target`, `draw_scaled_target`, `compute_scale_fit` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `create_hex_tilemap`, `create_infinite_tilemap`, `draw_tilemap` — [Tile Maps](../api_reference.md#tile-maps-includetilemaph) · [`tilemap.h`](../../include/tilemap.h)
- `create_atlas`, `sprite_sheet_from_atlas`, `Animator` — [Atlases](../api_reference.md#atlases--animated-sprites-includeatlash-includeanimated_spriteh) · [`atlas.h`](../../include/atlas.h)

## Source
[src/examples/ex2dfeatures/ex2dfeatures.cpp](../../src/examples/ex2dfeatures/ex2dfeatures.cpp)

## See also
- [excamera2d](excamera2d.md)
- [extilemap](extilemap.md)
- [exanimsprite](exanimsprite.md)
