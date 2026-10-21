# exgpuparticles

150,000 particles simulated entirely on the GPU by a compute shader, with gravity wells, explosions and an additive glow.

![exgpuparticles screenshot](img/exgpuparticles.png)

## What it shows
- Compute-shader simulation with `StorageBuffer` particle and force buffers
- Gravity wells and explosions as force sources
- Drawing particles with `draw_points` and additive blending
- Glow via `Blur`
- Live particle and active force counts

## Running
```bash
./build/exgpuparticles
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Requires compute shader support.

## Controls
| Input | Action |
|-------|--------|
| Left click / drag | Gravity well |
| Right click | Explosion |
| B | Toggle glow |
| Escape | Exit |

## Key APIs
- `StorageBuffer`, `Shader`, `ShaderUniform`, `dispatch_compute_for`, `compute_barrier` — [Shaders](../api_reference.md#shaders-compute--storage-buffers-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `draw_points`, `PointVertex`, `set_blend_mode` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `Blur` — [Post-Processing](../api_reference.md#post-processing-graphics-effects-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)

## Source
[src/examples/exgpuparticles/exgpuparticles.cpp](../../src/examples/exgpuparticles/exgpuparticles.cpp)

## See also
- [excompute](excompute.md)
- [exparticles](exparticles.md)

For architecture diagrams and shader details see the [GPU Compute Particles guide](../exgpuparticles_guide.md).
