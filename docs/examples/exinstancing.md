# exinstancing

Compares three ways to draw thousands of cubes: one call each, per-frame instance vectors, and a persistent `InstanceBuffer`.

![exinstancing screenshot](img/exinstancing.png)

## What it shows
- `draw_mesh` per object
- `draw_mesh_instanced` with a vector re-uploaded each frame
- `create_instance_buffer` uploaded once
- Frame-time readout for each mode

## Key code

In `exinstancing.cpp`, a persistent instance buffer is filled once (the third mode), while the other modes draw per object or re-upload a vector:

```cpp
std::vector<sl::InstanceData3D> instances(static_cast<std::size_t>(grid) * grid);
sl::InstanceBuffer *buffer = sl::create_instance_buffer(instances.size());
// ...
sl::update_instance_buffer(buffer, instances);
```

```cpp
if (mode == 0)
{
    for (int z = 0; z < grid; ++z)
        for (int x = 0; x < grid; ++x)
        {
            material.tint = cube_tint(x, z);
            sl::draw_mesh(cube, sl::model_matrix({(x - grid / 2) * 1.0f, wave(x, z, time), (z - grid / 2) * 1.0f}, {0, 0, 0}), material);
        }
}
else if (mode == 1)
{
    std::size_t index = 0;
    for (int z = 0; z < grid; ++z)
        for (int x = 0; x < grid; ++x, ++index)
            instances[index].model = sl::model_matrix({(x - grid / 2) * 1.0f, wave(x, z, time), (z - grid / 2) * 1.0f}, {0, 0, 0});
    sl::draw_mesh_instanced(cube, instances, material);
}
else
{
    sl::draw_mesh_instanced(cube, buffer, material);
}
```

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
