# exrotatesprite

Loads a bitmap and spins it with rotation and scaling at a fixed frame rate.

![exrotatesprite screenshot](img/exrotatesprite.png)

## What it shows
- Loading a PNG with `load_bitmap`
- Drawing a rotated, scaled sprite with `draw_sprite_rotated`
- Limiting the frame rate with `set_fps` and animating from `time_ms`
- Text overlays with `gprintf_center`

## Running
```bash
./build/exrotatesprite
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Loads its sprite from `assets/textures/`, so run from the repository root.

## Controls
| Input | Action |
|-------|--------|
| Escape, or closing the window | Exit |

## Key APIs
- `load_bitmap`, `draw_sprite_rotated`, `destroy_bitmap` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `set_fps`, `time_ms`, `poll_event` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)
- `set_gfx_mode`, `shutdown` — [Application Lifecycle](../api_reference.md#application-lifecycle--display-includedisplayh) · [`display.h`](../../include/display.h)

## Source
[src/examples/exrotatesprite/exrotatesprite.cpp](../../src/examples/exrotatesprite/exrotatesprite.cpp)

## See also
- [exbitmap](exbitmap.md)
- [exatlas](exatlas.md)
- [exanimsprite](exanimsprite.md)
