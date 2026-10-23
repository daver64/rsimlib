# exprops

A lit gallery of the low-poly scenery parts from `props3d.h`: 41 builders plus the conifer and rock from `clutter.h`, each shown on a plinth under an orbiting camera with sun shadows.

![exprops screenshot](img/exprops.png)

## What it shows
- Every `make_*_data()` builder: houses, offices, skyscrapers, hangars, walls, fences, water and control towers, radars, lamps, roads, barrels, crates, containers, tanks, trees, bushes, rocks, a car, a spaceship and an aeroplane, plus Apollo-era rocketry and spacecraft: a Saturn V and its stages, engines, adapters, fins, fairing, escape tower, command and service module, launch tower and pad, Skylab, station modules, solar arrays and a satellite
- Parts as plain `MeshData`: vertex colours carry the shading and baked windows, so a default `Material` draws them
- A sun shadow map and day/night lighting

## Key code

```cpp
sl::MeshData data = sl::make_control_tower_data(24.0f, 3);
sl::Mesh *mesh = sl::create_mesh(data.vertices, data.indices);
sl::draw_mesh(mesh, sl::model_matrix({x, 0.0f, z}, {0.0f, yaw, 0.0f}));
```

## Controls
Left/Right focus one part, `A` shows all, `[` / `]` zoom, `Space` pauses the orbit, `N` toggles day/night, right-drag looks around.

## Running
```
./build/exprops            # or --gl / --vulkan
```

Tested on OpenGL. See the lesson [Building a scene from parts](../tutorial/3d-06-building-a-scene-from-parts.md).
