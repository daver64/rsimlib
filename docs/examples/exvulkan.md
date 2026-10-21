# exvulkan

A Vulkan smoke test: a textured, rotating sprite and primitives rendered into a target with a bloom pass.

![exvulkan screenshot](img/exvulkan.png)

## What it shows
- Selecting the Vulkan backend in code
- `draw_sprite_rotated` and primitives
- `Bloom` post effect on a render target

## Key code

In `exvulkan.cpp`, the Vulkan backend is selected before the window is created:

```cpp
if (!sl::set_graphics_backend(sl::GraphicsBackend::vulkan) ||
    !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
{
    return -1;
}
```

The scene (rotated sprite and primitives) is drawn into a render target, then `Bloom` is applied to it:

```cpp
sl::Bloom bloom;
if (!bloom.initialise())
{
    sl::gprintf(0, 0, {255, 60, 60}, "Bloom init failed: %s", bloom.error().c_str());
}
bloom.set_threshold(0.4f);
bloom.set_intensity(1.2f);
// ...
sl::draw_sprite_rotated(balloon, 400.0f, 300.0f, static_cast<float>(sl::time_ms() % 3600) * 0.1f);
sl::line(sl::screen, 80.0f, 500.0f, 220.0f, 500.0f, {255, 180, 80});
sl::circlefill(sl::screen, 600.0f, 480.0f, 42.0f, {220, 100, 150});
if (has_scene)
    sl::end_render_target();

if (has_scene && bloom.is_valid())
{
    bloom.apply(scene, 0, 0, sl::screen_width(), sl::screen_height());
}
```

## Running
```bash
./build/exvulkan
```

Always uses the Vulkan backend; it takes no backend flag.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- `create_render_target`, `begin_render_target`, `end_render_target`, `draw_sprite_rotated` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `Bloom` — [Post-Processing](../api_reference.md#post-processing-graphics-effects-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)

## Source
[src/examples/exvulkan/exvulkan.cpp](../../src/examples/exvulkan/exvulkan.cpp)

## See also
- [ex3d_vulkan](ex3d_vulkan.md)
- [expostprocess](expostprocess.md)
