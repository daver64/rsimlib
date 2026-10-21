# exnormalmap

2D normal mapping: sprites lit by a mouse-driven light using per-sprite normal maps, with a toggle to compare against flat lighting.

![exnormalmap screenshot](img/exnormalmap.png)

## What it shows
- Attaching normal maps to sprites with `set_normal_map`
- Drawing sprites inside `begin_normal_pass` / `end_normal_pass`
- A `LightingPass` light that follows the mouse
- Procedurally built bitmaps via `create_bitmap` and `putpixel`

## Key code

In `exnormalmap.cpp`, the generated normal bitmap is attached to the brick sprite, and the lighting pass sets its ambient level and light height.
```cpp
    sl::LightingPass lighting;
    // ...
    make_bump_tile(brick, brick_normal);
    sl::set_normal_map(brick, brick_normal);
    lighting.set_ambient(0.12f);
    lighting.set_light_height(48.0f);
```

In `exnormalmap.cpp`, sprites are redrawn in the normal pass and a mouse-following light is applied to the scene and its normal map.
```cpp
        if (sl::begin_normal_pass(normals))
        {
            draw_tiles();
            sl::end_normal_pass();
        }

        sl::Light light;
        light.x = static_cast<float>(sl::mouse_x());
        light.y = static_cast<float>(sl::mouse_y());
        light.radius = 300.0f;
        light.intensity = 1.6f;
        light.colour = {255, 210, 150};

        if (use_normals)
        {
            lighting.apply(scene, normals, {light});
        }
```

## Running
```bash
./build/exnormalmap
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Mouse move | Move the light |
| Space | Toggle normal mapping on or off |
| Escape | Exit |

## Key APIs
- `set_normal_map`, `begin_normal_pass`, `end_normal_pass` — [2D Radial](../api_reference.md#2d-radial-lighting--polygon-shadows-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `LightingPass`, `Light` — [2D Radial](../api_reference.md#2d-radial-lighting--polygon-shadows-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `create_render_target`, `create_bitmap`, `putpixel` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `mouse_x`, `mouse_y` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)

## Source
[src/examples/exnormalmap/exnormalmap.cpp](../../src/examples/exnormalmap/exnormalmap.cpp)

## See also
- [exlighting](exlighting.md)
- [exnormalmap3d](exnormalmap3d.md)

See also the [Graphics Effects guide](../graphics_effects.md).
