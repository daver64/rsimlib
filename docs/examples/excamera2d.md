# excamera2d

A 2D camera with zoom, rotation and shake, plus clip rectangles, extended sprite parameters and tweens.

![excamera2d screenshot](img/excamera2d.png)

## What it shows
- `Camera2D` follow, zoom and rotation
- Camera shake
- `draw_sprite_ex` with `SpriteParams`
- Clip rectangles with `set_clip_rect` / `clear_clip_rect`
- `Tween` easing with `Ease`

## Key code

In `excamera2d.cpp`, a `Camera2D` is bounded to the world, a `Tween` is configured for easing, and each frame the camera follows the ship, zooms and rotates, with Space triggering the shake.

```cpp
sl::Camera2D camera;
camera.set_bounds(0.0f, 0.0f, world_size, world_size);
// ...
sl::Tween pulse;
pulse.from = 0.8f;
pulse.to = 1.4f;
pulse.duration = 0.8f;
pulse.easing = sl::Ease::sine_in_out;
pulse.ping_pong = true;
// ...
camera.shake(14.0f, 0.5f);
// ...
pulse.update(dt);
camera.follow(ship_x, ship_y, dt, 4.0f);
camera.update(dt);
```

Sprites are drawn through the camera using `SpriteParams`, with the tween value driving the scale.

```cpp
sl::SpriteParams p;
p.x = 200.0f + i * 110.0f;
p.y = 300.0f + std::fmod(i * 370.0f, 1000.0f);
p.rotation = angle * (i % 2 ? 1.0f : -1.0f);
p.scale_x = p.scale_y = pulse.value();
p.tint = {static_cast<Uint8>(120 + i * 10), 160, 255, 255};
camera.draw_sprite(ship, p);
```

The inset panel uses `set_clip_rect` so the large `draw_sprite_ex` sprite is cut off at the panel edges.

```cpp
sl::set_clip_rect(px, py, pw, ph);
sl::SpriteParams big;
big.x = px + pw * 0.5f;
big.y = py + ph * 0.5f;
big.scale_x = big.scale_y = 7.0f;
big.rotation = angle;
// ...
sl::draw_sprite_ex(ship, big);
sl::clear_clip_rect();
```

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
