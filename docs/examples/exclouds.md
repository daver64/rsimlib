# exclouds

A sky viewer for procedural cloud types, with altitude, sun position, planet curvature and volumetric rendering.

![exclouds screenshot](img/exclouds.png)

## What it shows
- Cirrus, cumulus and other cloud types
- Altitude changes
- Sun position
- Planet curvature on and off
- Volumetric cloud rendering

## Running
```bash
./build/exclouds [--types cirrus,cumulus,...]
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Set `RSIMLIB_SCREENSHOT=<file.png>` to save a screenshot on an early frame.

## Controls
| Input | Action |
|-------|--------|
| Arrow keys | Look |
| = / - | Altitude |
| T | Move the sun |
| P | Toggle planet |
| V | Toggle volumetric |
| 1-6 | Toggle cloud types |
| Escape | Exit |

## Key APIs
- Atmosphere and clouds — [Atmosphere](../api_reference.md#atmosphere--clutter-includeatmosphereh-includeclutterh) · [`atmosphere.h`](../../include/atmosphere.h)
- `Surface3D` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/exclouds/exclouds.cpp](../../src/examples/exclouds/exclouds.cpp)

## See also
- [exterrain](exterrain.md)
- [exflight](exflight.md)
