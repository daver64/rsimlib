# exvoxel

A Minecraft-style voxel world: fly around, break and place blocks, with chunk meshing.

![exvoxel screenshot](img/exvoxel.png)

## What it shows
- Chunked voxel meshing
- Block raycast targeting
- Breaking and placing blocks
- Mouse-look with a locked cursor
- Chunk and triangle counters

## Running
```bash
./build/exvoxel
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Click the window / Tab | Lock the mouse and look |
| W A S D | Fly |
| Space / C | Up / down |
| Shift | Fast |
| Left click | Break block |
| Right click | Place block |
| 1-9, 0 | Choose block |
| Tab / Escape | Free the cursor |
| Escape (cursor free) | Exit |

## Key APIs
- Voxel world — [Voxel Chunks](../api_reference.md#voxel-chunks--world-includevoxelh) · [`voxel.h`](../../include/voxel.h)
- `Surface3D` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/exvoxel/exvoxel.cpp](../../src/examples/exvoxel/exvoxel.cpp)

## See also
- [exterrain](exterrain.md)
- [exinstancing](exinstancing.md)
