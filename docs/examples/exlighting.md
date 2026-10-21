# exlighting

2D radial lights with polygon soft shadows, rendered through a `LightingPass` into a render target.

![exlighting screenshot](img/exlighting.png)

## What it shows
- Creating a `LightingPass` and `Light` objects
- Shadow casters built with `make_rectangle_shadow_caster`
- Rendering the scene into a render target and compositing the lit result
- Soft versus hard shadow modes and disabling shadows

## Running
```bash
./build/exlighting [--hard-shadows | --no-shadows]
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Escape, or closing the window | Exit |

## Key APIs
- `LightingPass`, `Light`, `ShadowCaster`, `make_rectangle_shadow_caster` — [2D Radial](../api_reference.md#2d-radial-lighting--polygon-shadows-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `create_render_target`, `begin_render_target`, `end_render_target` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `draw_sprite_rotated`, `rectfill` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/exlighting/exlighting.cpp](../../src/examples/exlighting/exlighting.cpp)

## See also
- [exnormalmap](exnormalmap.md)
- [expostprocess](expostprocess.md)
- [exrendertarget](exrendertarget.md)

See also the [Graphics Effects guide](../graphics_effects.md).
