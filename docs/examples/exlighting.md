# exlighting

2D radial lights with polygon soft shadows, rendered through a `LightingPass` into a render target.

![exlighting screenshot](img/exlighting.png)

## What it shows
- Creating a `LightingPass` and `Light` objects
- Shadow casters built with `make_rectangle_shadow_caster`
- Rendering the scene into a render target and compositing the lit result
- Soft versus hard shadow modes and disabling shadows

## Key code

In `exlighting.cpp`, a render target and a `LightingPass` are created, then ambient light and shadow casters are set up.

```cpp
sl::Bitmap *scene = sl::create_render_target(800, 600);
sl::LightingPass lighting;
if (!scene || !lighting.initialise())
// ...
lighting.set_ambient(0.18f);
const std::vector<sl::ShadowCaster> casters = {
    sl::make_rectangle_shadow_caster(300.0f, 370.0f, 500.0f, 400.0f),
    {{{170.0f, 390.0f}, {230.0f, 320.0f}, {290.0f, 390.0f}}},
};
```

Each frame the scene is rendered into the target, and `Light` objects are configured, with `shadow_softness` switching between soft and hard shadows.

```cpp
sl::begin_render_target(scene);
sl::clear_render_target({45, 48, 56});
// ...
sl::end_render_target();

sl::Light warm_light;
warm_light.radius = 320.0f;
warm_light.intensity = 1.4f;
warm_light.shadow_softness = hard_shadows ? 0.0f : 3.0f;
warm_light.colour = {255, 190, 110};
```

The lit result is composited with `apply`, passing no casters when shadows are disabled.

```cpp
lighting.apply(scene, lights, 0, 0, 800, 600,
               disable_shadows ? std::vector<sl::ShadowCaster>{} : casters);
```

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
