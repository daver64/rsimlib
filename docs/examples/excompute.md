# excompute

GPU compute: a compute shader updates 1,000 particles stored in a shader storage buffer, then the results are read back and drawn.

![excompute screenshot](img/excompute.png)

## What it shows
- Creating a `StorageBuffer` and uploading particle data
- Dispatching a compute shader with `dispatch_compute_for`
- Synchronising with `compute_barrier`
- Passing parameters with `ShaderUniform`
- Reading the buffer back to draw particles on the CPU side

## Key code

In `excompute.cpp`, the particle data is uploaded into a `StorageBuffer`, and `ShaderUniform` entries describe the push-constant layout used on Vulkan.

```cpp
static const std::vector<sl::ShaderUniform> compute_vulkan_uniforms = {
    {"uDeltaTime", 0, 4},
    {"uWidth", 4, 4},
    {"uHeight", 8, 4},
};
// ...
    sl::StorageBuffer particle_buffer(num_particles * sizeof(Particle));
    if (!particle_buffer.upload(host_particles))
    {
        std::cerr << "Failed to upload particle storage buffer.\n";
        return -1;
    }
```

Each frame, the shader is dispatched with `dispatch_compute_for`, synchronised with `compute_barrier`, and the results are read back for CPU-side drawing.

```cpp
        particle_buffer.bind(0);
        compute_shader.set_uniform("uDeltaTime", delta);
        compute_shader.set_uniform("uWidth", 800.0f);
        compute_shader.set_uniform("uHeight", 600.0f);
        sl::dispatch_compute_for(compute_shader, static_cast<unsigned int>(num_particles), 1, 1, 16, 1, 1);
        sl::compute_barrier();

        particle_buffer.readback(host_particles);
```

## Running
```bash
./build/excompute
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Requires a backend with compute support.

## Controls
| Input | Action |
|-------|--------|
| Closing the window | Exit |

## Key APIs
- `StorageBuffer`, `Shader`, `ShaderUniform` — [Shaders](../api_reference.md#shaders-compute--storage-buffers-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `dispatch_compute_for`, `compute_barrier` — [Shaders](../api_reference.md#shaders-compute--storage-buffers-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `circlefill`, `textout`, `set_window_title` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/excompute/excompute.cpp](../../src/examples/excompute/excompute.cpp)

## See also
- [exgpuparticles](exgpuparticles.md)
- [exshader](exshader.md)
- [exparticles](exparticles.md)

See also the [Shaders & compute reference](../api_reference.md#shaders-compute--storage-buffers-includegraphics_fxh).
