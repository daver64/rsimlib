# exflight

Aircraft flight with `FlightWorld`: lifting and control surfaces, a jet engine, landing gear on a runway and an exponential atmosphere.

![exflight screenshot](img/exflight.png)

## What it shows
- Forces from `LiftingSurface` coefficients, so stalls, trim and control authority emerge from the simulation
- A jet engine with throttle
- Landing gear, brakes and nosewheel steering on a runway
- Exponential atmosphere
- Switchable cameras
- A streaming `Terrain` with detail textures as the world and as the flight model's ground (`flat_terrain`), plus forests and boulders from `ClutterLayer`
- Atmosphere, sky and clouds from `Environment3D`
- A lathe-turned jet model with wispy exhaust smoke
- Synthesised jet and wind sound loops driven by throttle and airspeed with `set_voice_volume`

## Key code

In `exflight.cpp`, the aircraft's forces come from `LiftingSurface` coefficients, and the exponential atmosphere and RK4 integrator are configured on the `FlightWorld`.

```cpp
        world.flat_atmosphere.surface_density = 1.225;
        world.integrator = sl::FlightIntegrator::rk4;
// ...
        b.surfaces.push_back(make_surface("wing", {-2.5, 0.0, 0.0}, {0, 1, 0}, 8.0, 4.6, 0.3, 0.0));
        b.surfaces.push_back(make_surface("wing", {2.5, 0.0, 0.0}, {0, 1, 0}, 8.0, 4.6, 0.3, 0.0));
        // Ailerons on the outer wings: positive control rolls right.
        b.surfaces.push_back(make_surface("aileron", {-4.5, 0.0, 0.0}, {0, 1, 0}, 1.4, 3.5, 0.0, 0.18));
```

The jet engine is a `Thruster`, and the landing gear is a set of `GearContact` spring-damper wheels.

```cpp
        sl::Thruster jet;
        jet.position = {0.0, 0.0, 1.0};
        jet.direction = {0.0, 0.0, -1.0};
        jet.max_thrust = 4500.0;
        jet.air_breathing = true;
        b.thrusters.push_back(jet);
// ...
            sl::GearContact g;
            g.position = p;
            g.stiffness = 6.0e4;
            g.damping = 8.0e3;
            b.gear.push_back(g);
```

Each frame, the throttle, control surfaces, brakes and nosewheel steering are driven from the keyboard before stepping the simulation.

```cpp
        world.set_control(id, "elevator", axis(sl::Key::arrow_up, sl::Key::arrow_down));
        world.set_control(id, "aileron", axis(sl::Key::arrow_right, sl::Key::arrow_left));
        world.set_control(id, "rudder", rudder);
        world.set_brakes(id, sl::key_down(sl::Key::letter_b) ? 1.0 : 0.0);
        // Nosewheel steering follows the rudder pedals.
        plane.gear[0].forward = {std::sin(-rudder * 0.5), 0.0, -std::cos(rudder * 0.5)};

        world.step(dt);
```

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
| M | Sound on / off |
| R | Reset |
| Escape | Exit |

## Key APIs
- `FlightWorld`, `LiftingSurface` — [3D Flight](../api_reference.md#3d-flight--orbital-dynamics-includeflighth) · [`flight.h`](../../include/flight.h)
- Audio — [`set_voice_volume`](../api_reference.md#audio-subsystem-samples--streams-includeaudioh) · [`audio.h`](../../include/audio.h)
- Terrain and atmosphere — [Chunked-LOD](../api_reference.md#chunked-lod-terrain-includeterrainh) · [`terrain.h`](../../include/terrain.h)

## Source
[src/examples/exflight/exflight.cpp](../../src/examples/exflight/exflight.cpp)

## See also
- [exorbit](exorbit.md)
- [exterrain](exterrain.md)
- [exmodel3d](exmodel3d.md)

See also the [Flight guide](../flight.md).
