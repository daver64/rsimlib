# exclouds

A sky viewer for procedural cloud types, with altitude, sun position, planet curvature and volumetric rendering.

![exclouds screenshot](img/exclouds.png)

## What it shows
- Cirrus, cumulus and other cloud types
- Altitude changes
- Sun position
- Planet curvature on and off
- Volumetric cloud rendering

## Key code

In `exclouds.cpp`, an `Environment3D` is built each frame from the altitude, sun elevation and planet toggle, then enabled cloud types are added as layers (volumetric can be switched off):

```cpp
sl::Environment3D environment;
environment.sun_direction = glm::normalize(glm::vec3(-0.5f, -sun_elevation, 0.4f));
environment.camera_altitude = altitude;
environment.camera_position = {0.0, altitude, 0.0};
if (planet)
{
    environment.planet = true;
    environment.sun_intensity = 20.0f;
}
int layers = 0;
for (const Entry &entry : entries)
{
    if (!entry.on || layers >= 4)
        continue;
    sl::CloudLayer layer = sl::cloud_layer(entry.type);
    // With volumetric off, the 3D types fall back to their flat layer.
    if (!volumetric)
        layer.volumetric = false;
    (layers == 0 ? environment.clouds : environment.more_clouds[layers - 1]) = layer;
    ++layers;
}
```

The environment is applied to the 3D surface and the sky is drawn:

```cpp
sl::set_environment_3d(environment);
sl::draw_sky_3d();
sl::Light3D light;
light.direction = environment.sun_direction;
light.colour = sl::atmosphere_sun_colour(environment);
```

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
