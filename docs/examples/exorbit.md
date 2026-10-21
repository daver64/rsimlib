# exorbit

Spaceflight: a craft with main engine, retro thrust and RCS, orbit prediction, time warp and a Moon on rails.

![exorbit screenshot](img/exorbit.png)

## What it shows
- Orbital dynamics and trajectory prediction (periapsis, escape trajectories)
- Main engine, throttle, retro burn and RCS translation and rotation
- SAS stability assist
- Time warp
- Free-look and zoom camera

## Running
```bash
./build/exorbit
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Z / X | Main engine on / off |
| Shift / Ctrl | Throttle up / down |
| B | Retro burn |
| W A S D Q E | Rotate |
| Arrow keys | Translate |
| T | SAS |
| O | Toggle orbit display |
| , / . | Time warp |
| Right-drag | Look |
| [ / ] | Zoom |
| Escape | Exit |

## Key APIs
- `FlightWorld` orbital mechanics — [3D Flight](../api_reference.md#3d-flight--orbital-dynamics-includeflighth) · [`flight.h`](../../include/flight.h)

## Source
[src/examples/exorbit/exorbit.cpp](../../src/examples/exorbit/exorbit.cpp)

## See also
- [exflight](exflight.md)
- [exterrain](exterrain.md)

See also the [Flight guide](../flight.md).
