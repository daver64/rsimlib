# exmodel3d

A model viewer for glTF/GLB, AC3D and FlightGear XML models with clip playback and property-driven animations.

![exmodel3d screenshot](img/exmodel3d.png)

## What it shows
- `load_model` and `load_flightgear_model`
- Clip playback (`play_model_clip`, `update_model_clip`)
- External animation JSON with `load_model_animations_json`
- Property channels driving part visibility and animation via `set_model_property`
- Orbit camera over a `Surface3D`

## Running
```bash
./build/exmodel3d [model.glb|model.ac|flightgear-model.xml] [--animations file.json]
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Set `RSIMLIB_SCREENSHOT=<file.png>` to save a screenshot on an early frame.

## Controls
| Input | Action |
|-------|--------|
| Arrow keys | Orbit |
| + / - | Zoom |
| C | Next clip |
| P | Pause |
| L | Toggle loop |
| S / F | Slower / faster |
| Tab / PgUp / PgDn | Select a property channel |
| Space | Toggle the channel |
| , / . | Adjust the channel value |
| Home | Reset channels |
| Escape | Exit |

## Key APIs
- `load_model`, `load_flightgear_model`, `draw_model` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `play_model_clip`, `update_model_clip`, `set_model_property` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/exmodel3d/exmodel3d.cpp](../../src/examples/exmodel3d/exmodel3d.cpp)

## See also
- [ex3d_surface](ex3d_surface.md)
- [exflight](exflight.md)
