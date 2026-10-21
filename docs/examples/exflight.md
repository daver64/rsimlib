# exflight

Aircraft flight with `FlightWorld`: lifting and control surfaces, a jet engine, landing gear on a runway and an exponential atmosphere.

![exflight screenshot](img/exflight.png)

## What it shows
- Forces from `LiftingSurface` coefficients, so stalls, trim and control authority emerge from the simulation
- A jet engine with throttle
- Landing gear, brakes and nosewheel steering on a runway
- Exponential atmosphere
- Switchable cameras

## Running
```bash
./build/exflight
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Shift / Ctrl | Throttle up / down |
| Up / Down arrows | Stick forward / back |
| Left / Right arrows | Roll |
| A / D | Rudder and nosewheel steering |
| B | Brakes |
| C | Change camera |
| R | Reset |
| Escape | Exit |

## Key APIs
- `FlightWorld`, `LiftingSurface` — [3D Flight](../api_reference.md#3d-flight--orbital-dynamics-includeflighth) · [`flight.h`](../../include/flight.h)
- Terrain and atmosphere — [Chunked-LOD](../api_reference.md#chunked-lod-terrain-includeterrainh) · [`terrain.h`](../../include/terrain.h)

## Source
[src/examples/exflight/exflight.cpp](../../src/examples/exflight/exflight.cpp)

## See also
- [exorbit](exorbit.md)
- [exterrain](exterrain.md)
- [exmodel3d](exmodel3d.md)

See also the [Flight guide](../flight.md).
