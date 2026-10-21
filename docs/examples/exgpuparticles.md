# exgpuparticles

150,000 particles simulated entirely on the GPU by a compute shader, with gravity wells, explosions and an additive glow.

![exgpuparticles screenshot](img/exgpuparticles.png)

## What it shows
- Compute-shader simulation with `StorageBuffer` particle and force buffers
- Gravity wells and explosions as force sources
- Drawing particles with `draw_points` and additive blending
- Glow via `Blur`
- Live particle and active force counts

## Key code

In `exgpuparticles.cpp`, particle and force data live in `StorageBuffer`s, and each frame the forces are uploaded and the compute shader is dispatched:

```cpp
sl::StorageBuffer particle_buffer(NUM_PARTICLES * sizeof(GpuParticle));
// ...
sl::StorageBuffer force_buffer(MAX_FORCES * sizeof(ForceSource));
force_buffer.upload(host_forces);
// ...
compute_shader.use();
particle_buffer.bind(0);
force_buffer.bind(1);
compute_shader.set_uniform("uDeltaTime", delta);
compute_shader.set_uniform("uForceCount", force_count);
sl::dispatch_compute_for(compute_shader, static_cast<unsigned int>(NUM_PARTICLES), 1, 1, 256, 1, 1);
sl::compute_barrier();

particle_buffer.readback(host_particles);
```

Particles are drawn with `draw_points` under additive blending, then a blurred copy of the scene is added for the glow:

```cpp
sl::set_blend_mode(sl::BlendMode::additive);
sl::draw_points(sl::screen, points.data(), static_cast<int>(points.size()));
sl::set_blend_mode(sl::BlendMode::normal);
// ...
if (use_glow && glow_ready)
{
    sl::set_blend_mode(sl::BlendMode::additive);
    glow.apply(scene, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sl::set_blend_mode(sl::BlendMode::normal);
}
```

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
