# extriangle

Immediate-mode 2D primitives: outlined and filled triangles drawn over a loaded bitmap.

![extriangle screenshot](img/extriangle.png)

## What it shows
- Drawing outlined triangles with `triangle`
- Drawing filled triangles with `trianglefill`
- Loading a bitmap with `load_bitmap`
- Centred text labels and the renderer label overlay

## Running
```bash
./build/extriangle
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Any key, or closing the window | Exit |

## Key APIs
- `triangle`, `trianglefill`, `clear_to_colour` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `load_bitmap`, `destroy_bitmap` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `gprintf_center` — [Typography](../api_reference.md#typography--text-rendering-includefonth) · [`font.h`](../../include/font.h)

## Source
[src/examples/extriangle/extriangle.cpp](../../src/examples/extriangle/extriangle.cpp)

## See also
- [exhello](exhello.md)
- [exbitmap](exbitmap.md)
- [exrotatesprite](exrotatesprite.md)
