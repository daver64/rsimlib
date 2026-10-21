# exfluid3d

3D voxel fluid sandbox: spray materials into a voxel volume rendered as a mesh with an orbit camera.

![exfluid3d screenshot](img/exfluid3d.png)

## What it shows
- 3D cellular-automata fluid in a voxel grid
- Spraying elements with the mouse
- Live cell, changed-voxel and triangle counters
- Orbit, tilt and zoom camera

## Key code

In `exfluid3d.cpp`, a camera ray selects a voxel surface and the chosen brush writes cells into the 3D fluid grid before it is exported to the voxel world.
```cpp
        const sl::VoxelRaycastResult hit = world.raycast(eye, ray_dir, 200.0f);

        if (hit.hit && sl::mouse_button_down(sl::MouseButton::left))
        {
            const Brush &b = BRUSHES[brush];
            for (int dz = 0; dz < 2; ++dz)
                for (int dx = 0; dx < 2; ++dx)
                {
                    const int gx = hit.place_block.x - ORIGIN_X + dx;
                    const int gy = hit.place_block.y + 1;
                    const int gz = hit.place_block.z - ORIGIN_Z + dz;
                    if (gx > 0 && gy > 0 && gz > 0 && gx < GRID_W - 1 && gy < GRID_H - 1 && gz < GRID_D - 1 &&
                        sl::fluid3d_get_cell(sim, gx, gy, gz).type == sl::FluidElement::empty)
                        sl::fluid3d_set_cell(sim, gx, gy, gz, b.element, 1.0f, b.temp, b.life);
                }
        }

        if (!paused)
            sl::fluid3d_step(sim);
        const std::size_t changed = sl::fluid3d_export_to_voxels(sim, world, fluid_blocks);
        world.update_dirty_chunks();
```
## Running
```bash
./build/exfluid3d
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Left click | Spray |
| 1-9, 0 | Choose element |
| Space | Pause |
| R | Reset |
| A / D | Orbit |
| W / S | Tilt |
| Q / E | Zoom |
| Escape | Exit |

## Key APIs
- 3D fluid — [3D Fluid](../api_reference.md#3d-fluid-simulation-includefluid3dh) · [`fluid3d.h`](../../include/fluid3d.h)
- Voxel meshes — [Voxel Chunks](../api_reference.md#voxel-chunks--world-includevoxelh) · [`voxel.h`](../../include/voxel.h)

## Source
[src/examples/exfluid3d/exfluid3d.cpp](../../src/examples/exfluid3d/exfluid3d.cpp)

## See also
- [excafluid](excafluid.md)
- [exvoxel](exvoxel.md)
