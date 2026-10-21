# exinstancing

Compares three ways to draw thousands of cubes: one call each, per-frame instance vectors, and a persistent `InstanceBuffer`.

![exinstancing screenshot](img/exinstancing.png)

## What it shows
- `draw_mesh` per object
- `draw_mesh_instanced` with a vector re-uploaded each frame
- `create_instance_buffer` uploaded once
- Frame-time readout for each mode

## Running
```bash
./build/exinstancing
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Space | Switch draw mode |
| Escape | Exit |

## Key APIs
- `draw_mesh_instanced`, `create_instance_buffer` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `create_surface_3d` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/exinstancing/exinstancing.cpp](../../src/examples/exinstancing/exinstancing.cpp)

## See also
- [ex3d_surface](ex3d_surface.md)
- [exvoxel](exvoxel.md)
