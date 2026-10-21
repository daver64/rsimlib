# exrendertarget

Offscreen render targets: draws shapes into a render target and then draws that target to the screen as a sprite.

![exrendertarget screenshot](img/exrendertarget.png)

## What it shows
- Creating a target with `create_render_target`
- Drawing into it between `begin_render_target` and `end_render_target`
- Using shape primitives: `rectfill`, `circlefill`, `trianglefill`
- Drawing the result with `draw_sprite`

## Running
```bash
./build/exrendertarget
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Escape, or closing the window | Exit |

## Key APIs
- `create_render_target`, `begin_render_target`, `clear_render_target`, `end_render_target` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `rectfill`, `circlefill`, `trianglefill`, `draw_sprite` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/exrendertarget/exrendertarget.cpp](../../src/examples/exrendertarget/exrendertarget.cpp)

## See also
- [expostprocess](expostprocess.md)
- [exlighting](exlighting.md)
- [exbitmap](exbitmap.md)
