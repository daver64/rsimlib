# exterrain

A fly-over procedural terrain with normals, atmosphere, clouds, volumetrics, and instanced trees and rocks.

![exterrain screenshot](img/exterrain.png)

## What it shows
- Procedural terrain generation and reseeding
- Normal visualisation
- Atmosphere and sun control
- Cloud layers and volumetric clouds
- Trees and rocks as objects and billboards with draw statistics

## Running
```bash
./build/exterrain
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| W A S D | Move |
| Space / C | Up / down |
| Shift | Fast |
| 1-4 | Speed |
| R | New seed |
| N | Toggle normals |
| H | Toggle atmosphere |
| K | Toggle clouds |
| V | Toggle volumetrics |
| T | Toggle trees |
| Z / X | Move the sun |
| Escape | Exit |

## Key APIs
- Terrain — [Chunked-LOD](../api_reference.md#chunked-lod-terrain-includeterrainh) · [`terrain.h`](../../include/terrain.h)
- Atmosphere and clouds — [Atmosphere](../api_reference.md#atmosphere--clutter-includeatmosphereh-includeclutterh) · [`atmosphere.h`](../../include/atmosphere.h)
- `Surface3D` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- Noise — [Procedural FastNoise](../api_reference.md#procedural-fastnoise-generation-includenoiseh) · [`noise.h`](../../include/noise.h)

## Source
[src/examples/exterrain/exterrain.cpp](../../src/examples/exterrain/exterrain.cpp)

## See also
- [exclouds](exclouds.md)
- [exflight](exflight.md)
- [exvoxel](exvoxel.md)

The same scene on the Vulkan backend (`--vulkan`):

![exterrain on Vulkan](img/exterrain_vulkan.png)
