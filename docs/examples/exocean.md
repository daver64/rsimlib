# exocean

An ocean of rolling waves under volumetric clouds, with an early 20th century dreadnought heaving, pitching and rolling on the swell.

![exocean screenshot](img/exocean.png)

## What it shows
- A Gerstner-wave sea (the shared `sl::Ocean` class, also used by `exterrain` for shorelines): six wave trains of different wavelength and direction, with sharp crests, analytic normals and foam where the surface pinches
- A polar grid mesh that follows the ship, dense close in and sparse towards the horizon, updated every frame with `update_mesh_vertices`; short waves fade out with distance so they never alias
- A glossy PBR water material that picks up sun glitter, with `Environment3D` planet atmosphere, fog and sunsets
- A ray-marched cumulus layer under a cirrus layer, with adjustable cover
- A ship that samples the wave height at its bow, stern and sides and follows them with damped springs for heave, pitch and roll
- A custom lofted hull mesh with a red/grey colour split at the waterline, four turrets with twin barrels, a conning tower and tripod mast, two funnels with smoke and casemate guns
- Synthesised wind, surf and engine loops whose volume follows the sea state
- Floating-origin rendering: the camera stays at the origin and everything is drawn relative to the eye

## Key code

The same wave function drives the mesh and the ship, so the hull always sits on the visible water.

```cpp
const double theta = w.k * (w.dir_x * px + w.dir_z * pz) - w.omega * t + w.phase;
p.x += q * a * w.dir_x * std::cos(theta);
p.z += q * a * w.dir_z * std::cos(theta);
p.y += a * std::sin(theta);
```

Each frame the sea is rebuilt around the ship and uploaded, then drawn with an identity matrix because positions are already eye-relative.

```cpp
sea_mesh.update(sea, centre_x, centre_z, eye, clock);
sl::draw_mesh(sea_mesh.mesh, glm::mat4(1.0f), water);
```

## Running
```
./build/exocean            # or --gl / --vulkan / --d3d12
```

## Controls
- Up / Down: sea state
- Left / Right: cloud cover
- Q / E: sun elevation (down to sunset)
- Right-drag: look around the ship; `[` / `]`: zoom
- P: pause, M: sound, Esc: exit

## Key APIs
- `sl::Ocean` (`set_sea_state`, `update`, `draw`, `height_at`, `swell_height`)
- `sl::create_mesh`, `sl::update_mesh_vertices`, `sl::draw_mesh`
- `sl::Environment3D`, `sl::cloud_layer`, `sl::draw_sky_3d`, `sl::atmosphere_sun_colour`
- `sl::Material` (`pbr`, `roughness`, `cull`, `blend`)
- `sl::set_voice_volume`

## Source
[src/examples/exocean/exocean.cpp](../../src/examples/exocean/exocean.cpp)

## See also
[exorbit](exorbit.md), [exflight](exflight.md), [exprecip3d](exprecip3d.md)
