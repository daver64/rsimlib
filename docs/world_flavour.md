# World Flavour: Atmosphere, Clouds & Clutter

Sky, fog, clouds and scattered objects for 3D scenes, built to work with [Terrain](terrain.md) on flat arenas and planets. Headers: [atmosphere.h](../include/atmosphere.h) and [clutter.h](../include/clutter.h). Both work on the OpenGL, Vulkan and Direct3D 12 backends (Direct3D 12 is untested at the time of writing).

## Atmosphere

Describe the air with an `Environment3D`, set it after `begin_surface_3d()`, and draw the sky before other geometry:

```cpp
sl::Environment3D env;
env.sun_direction = sun_travel_direction;   // direction the light travels
env.up = up;                                // local vertical
env.camera_altitude = altitude;
env.camera_position = world_position;       // double precision; keeps clouds fixed in the world
env.time = seconds;
env.fog_density = 1.4e-4f;                  // flat worlds: exponential fog
env.clouds.enabled = true;
sl::set_environment_3d(env);
sl::draw_sky_3d();
light.colour = sl::atmosphere_sun_colour(env);   // reddens toward the horizon
```

- **Flat worlds** use a zenith/horizon gradient and exponential fog that thins with altitude (`fog_height_falloff`).
- **Planets** (`env.planet = true`, with `planet_radius`, `atmosphere_height`, `rayleigh`, `mie`) use Rayleigh/Mie scattering for the sky, sunsets and the haze on distant terrain. `haze_scale` tunes how quickly mountains fade.
- **Volumetric clouds**: set `env.clouds.volumetric = true` (and `thickness`, default 1500 m) to ray march a real slab of cloud above `altitude`, with puffy tops, darker bases, lit edges and a silver lining toward the sun. It works on flat worlds and planets and you can fly into the slab. It costs more GPU time than the flat layer. The clouds are part of the sky, so terrain always hides them (distant mountains in front of cloud are right, cloud in front of a mountain is not), and meshes inside the slab are not shaded by it.
- **Clouds** are a scrolling 2D layer (`env.clouds`: altitude, coverage, density, scale, softness, wind).
- Materials fade into the fog unless `Material::fog` is false.
- `atmosphere_sky_colour`, `atmosphere_sun_colour` and `atmosphere_transmittance` mirror the shader on the CPU.
- `clear_environment_3d()` turns it all off.

## Clutter

`bake_instances`, `bake_impostor` and `bake_billboards` build trees, rocks and the like into a few draw calls (the backends have no hardware instancing, so groups are merged on the CPU). `make_conifer_data` and `make_rock_data` give procedural starting shapes. `Material::alpha_cutoff` gives hard-edged cut-outs without sorting.

### Scattering over terrain: `ClutterLayer`

```cpp
sl::ClutterLayer clutter(terrain);          // the terrain must outlive it

sl::ClutterSpecies trees;
trees.mesh = sl::make_conifer_data(9.0f, 2.2f);
trees.density = 0.004f;                     // per square metre
trees.max_height = 90.0f;                   // above the terrain's base level
trees.max_slope = 30.0f;                    // degrees
trees.near_distance = 220.0f;               // full meshes closer than this
trees.far_distance = 1400.0f;               // impostors up to here
trees.suitability = [](const sl::TerrainSample &s) { return s.height < 60.0 ? 1.0f : 0.3f; };
clutter.add_species(trees);                 // bakes the impostor: call outside begin_surface_3d

clutter.update(view);                       // every frame, outside begin_surface_3d
terrain.draw(origin);
clutter.draw(origin);                       // inside begin_surface_3d / end_surface_3d
```

- Placement is deterministic per cell and species (`seed`), so objects stay put as you move.
- The ground is split into square cells (`ClutterConfig::cell_size`). Near cells hold a mesh of every object; farther cells draw billboards of an impostor that turns to face the camera in 8 steps.
- On a planet, objects stand on the local up and are placed relative to each cell, so precision holds at planet scale. `lean` tilts them toward the surface normal.
- Work happens in `update()` on the calling thread, limited by `time_budget_ms` per frame, so a new area fills in over a few frames. Cells unused for longest are dropped beyond `max_cached_cells`.
- After editing the terrain, call `invalidate_region()` (flat) or `invalidate_all()`.
- Impostors are lit once at bake time and drawn unlit; they still take the fog.

See [exterrain](../src/examples/exterrain/exterrain.cpp): key `T` toggles trees and rocks, `H` the atmosphere, `K` the clouds, `V` volumetric clouds, `Z`/`X` the sun.
