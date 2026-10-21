# exparticles

CPU particle emitter that follows the mouse, with gravity, blur and glow post effects.

![exparticles screenshot](img/exparticles.png)

## What it shows
- `ParticleEmitter` configured with `EmitterConfig`
- Activating, clearing and moving the emitter
- Gravity on and off
- Blur and glow effects applied to a render target
- Live particle count

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
