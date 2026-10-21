# exvulkan

A Vulkan smoke test: a textured, rotating sprite and primitives rendered into a target with a bloom pass.

![exvulkan screenshot](img/exvulkan.png)

## What it shows
- Selecting the Vulkan backend in code
- `draw_sprite_rotated` and primitives
- `Bloom` post effect on a render target

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
