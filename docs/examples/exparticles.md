# exparticles

CPU particle emitter that follows the mouse, with gravity, blur and glow post effects.

![exparticles screenshot](img/exparticles.png)

## What it shows
- `ParticleEmitter` configured with `EmitterConfig`
- Activating, clearing and moving the emitter
- Gravity on and off
- Blur and glow effects applied to a render target
- Live particle count

## Key code

In `exparticles.cpp`, an `EmitterConfig` describes the particles and a `ParticleEmitter` is built from it:

```cpp
sl::EmitterConfig config;
config.emit_rate = 180.0f;
config.particle_lifetime = 1.6f;
config.min_speed = 40.0f;
config.max_speed = 180.0f;
config.direction_degrees = -90.0f;
config.spread_degrees = 34.0f;
config.start_colour = {255, 220, 105, 235};
config.end_colour = {235, 75, 48, 0};
config.gravity = 95.0f;
config.max_particles = 900;

sl::ParticleEmitter emitter(config, 400.0f, 500.0f);
```

Each frame the emitter follows the mouse, is updated and rendered into a render target, and glow or blur is applied to that target:

```cpp
emitter.set_position(static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()));
emitter.update(elapsed);
// ...
sl::begin_render_target(particles);
sl::clear_render_target({0, 0, 0, 0});
emitter.render();
sl::end_render_target();
// ...
if (use_glow)
{
    glow.apply(particles, 0, 0, 800, 600);
}
```

## Running
```bash
./build/exparticles
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Mouse move | Move the emitter |
| Space | Start / stop emitting |
| G | Toggle gravity |
| C | Clear particles |
| B | Toggle blur |
| O | Toggle glow |
| + / - | Change effect radius |
| Escape | Exit |

## Key APIs
- `ParticleEmitter`, `EmitterConfig` — [Particle Systems](../api_reference.md#particle-systems--emitters-includeparticlesh) · [`particles.h`](../../include/particles.h)
- `Blur` — [Post-Processing](../api_reference.md#post-processing-graphics-effects-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `create_render_target`, `begin_render_target`, `end_render_target`, `draw_sprite` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/exparticles/exparticles.cpp](../../src/examples/exparticles/exparticles.cpp)

## See also
- [exgpuparticles](exgpuparticles.md)
- [exanimsprite](exanimsprite.md)
- [expostprocess](expostprocess.md)
