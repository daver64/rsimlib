# excamera2d

A 2D camera with zoom, rotation and shake, plus clip rectangles, extended sprite parameters and tweens.

![excamera2d screenshot](img/excamera2d.png)

## What it shows
- `Camera2D` follow, zoom and rotation
- Camera shake
- `draw_sprite_ex` with `SpriteParams`
- Clip rectangles with `set_clip_rect` / `clear_clip_rect`
- `Tween` easing with `Ease`

## Running
```bash
./build/excamera2d
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Set `RSIMLIB_SCREENSHOT=<file.png>` to save a screenshot on an early frame.

## Controls
| Input | Action |
|-------|--------|
| Arrow left / right | Steer |
| Arrow up | Move faster |
| Z / X | Zoom in / out |
| Q / E | Rotate |
| Space | Camera shake |
| Escape | Exit |

## Key APIs
- `Camera2D`, `Tween`, `Ease`, `SpriteParams`, `draw_sprite_ex` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `set_clip_rect`, `clear_clip_rect` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `key_down`, `get_frame_time` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)

## Source
[src/examples/excamera2d/excamera2d.cpp](../../src/examples/excamera2d/excamera2d.cpp)

## See also
- [ex2dfeatures](ex2dfeatures.md)
- [exisometric](exisometric.md)
- [exrotatesprite](exrotatesprite.md)
