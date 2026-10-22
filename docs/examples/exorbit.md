# exorbit

Spaceflight: a craft with main engine, retro thrust and RCS, orbit prediction, time warp and a Moon on rails.

![exorbit screenshot](img/exorbit.png)

## What it shows
- Orbital dynamics and trajectory prediction (periapsis, escape trajectories)
- Main engine, throttle, retro burn and RCS translation and rotation
- SAS stability assist
- Time warp
- Free-look and zoom camera
- A procedural streaming planet (`Terrain` on a sphere) with atmosphere and a cloud shell, and a textured Moon
- An Apollo-style command and service module with glowing additive exhaust particles from every firing thruster
- Synthesised rocket and RCS sound loops driven by thruster levels with `set_voice_volume`
- Metre units with a floating origin: the camera stays at the origin and everything is drawn relative to the eye

## Key code

In `exorbit.cpp`, the ship is given a main engine and a retro thruster (with thrust, specific impulse and a fuel tank), and a standard thruster group set is built.

```cpp
sl::Thruster main;
main.direction = {0, 0, -1};
main.position = {0, 0, 4};
main.max_thrust = 20000.0;
main.isp = 320.0;
main.tank = 0;
ship->thrusters.push_back(main);
sl::Thruster retro = main;
retro.direction = {0, 0, 1};
retro.position = {0, 0, -4};
retro.max_thrust = 5000.0;
ship->thrusters.push_back(retro);
// ...
w.build_standard_groups(s.ship);
```

Each frame, throttle, retro burn, RCS rotation (with SAS damping the spin) and translation are applied, then the world is stepped by the frame time scaled by the time-warp factor.

```cpp
if (sl::key_down(sl::Key::left_shift))
    world.set_group_level(scenario.ship, "main", std::min(1.0, ship.thrusters[0].level + throttle_rate));
world.set_group_level(scenario.ship, "retro", sl::key_down(sl::Key::letter_b) ? 1.0 : 0.0);
// ...
if (sas && glm::length(rotation) == 0.0)
    rotation = glm::clamp(-ship.angular_velocity * 30.0, sl::dvec3(-1.0), sl::dvec3(1.0));
world.set_rcs_rotation(scenario.ship, rotation.x, rotation.y, rotation.z);
world.set_rcs_translation(scenario.ship, axis(sl::Key::arrow_right, sl::Key::arrow_left),
                          axis(sl::Key::arrow_up, sl::Key::arrow_down), 0.0);
// ...
const double warp_factor = thrusting ? 1.0 : warp_levels[warp];
world.step(frame * warp_factor);
```

The free-look camera orbits the ship with the right mouse button and zooms with the bracket keys, and the predicted orbit comes from `orbit_of` and `orbit_path`.

```cpp
if (sl::mouse_button_down(sl::MouseButton::right))
{
    cam_yaw -= static_cast<float>(mx - last_mouse_x) * 0.008f;
    cam_pitch = std::clamp(cam_pitch + static_cast<float>(my - last_mouse_y) * 0.008f, -1.5f, 1.5f);
}
// ...
sl::OrbitElements elements;
const bool have_orbit = world.orbit_of(scenario.ship, scenario.earth, elements);
// ...
const std::vector<sl::dvec3> path = sl::orbit_path(earth_gm, elements, 360, 2.0e9);
```

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
| M | Sound on / off |
| Escape | Exit |

## Key APIs
- Terrain and atmosphere — [Chunked-LOD](../api_reference.md#chunked-lod-terrain-includeterrainh) · [`terrain.h`](../../include/terrain.h)
- `FlightWorld` orbital mechanics — [3D Flight](../api_reference.md#3d-flight--orbital-dynamics-includeflighth) · [`flight.h`](../../include/flight.h)

## Source
[src/examples/exorbit/exorbit.cpp](../../src/examples/exorbit/exorbit.cpp)

## See also
- [exflight](exflight.md)
- [exterrain](exterrain.md)

See also the [Flight guide](../flight.md).
