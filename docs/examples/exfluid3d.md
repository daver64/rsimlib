# exfluid3d

3D voxel fluid sandbox: spray materials into a voxel volume rendered as a mesh with an orbit camera.

![exfluid3d screenshot](img/exfluid3d.png)

## What it shows
- 3D cellular-automata fluid in a voxel grid
- Spraying elements with the mouse
- Live cell, changed-voxel and triangle counters
- Orbit, tilt and zoom camera

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
