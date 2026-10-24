# Showcase Guide

See also the [Example Gallery](examples/README.md) for a screenshot and controls page for each example.

[← Back to README](../README.md)

The [Tutorial](tutorial/README.md) covers the basics lesson by lesson.

simlib's demonstrations are independent applications rather than one large sample. Start with these three purpose-built showcases, then use the focused examples listed below to explore an individual subsystem. The [README example list](../README.md#running-the-examples) is the complete executable catalog.

## The Three Showcase Apps

### `showcase2d` — Lightkeeper

```bash
cmake --build build --target showcase2d
./build/showcase2d
```

Collect the six crystals in a small top-down arena. Move with **WASD** or the **arrow keys**, press **R** to reset, and **Escape** to exit. The demo combines Box2D movement and wall collisions, a particle trail, dynamic colored lights, and polygon shadow casters.

### `showcase3d` — Living World

```bash
cmake --build build --target showcase3d
./build/showcase3d
```

Fly over streamed procedural terrain. Use **WASD** to move, **Space/C** to rise or fall, **right mouse drag** to look, **Shift** to boost, and **1–3** to select speed. Toggle **H** atmosphere/fog, **K** clouds, and **T** scattered trees; **Escape** exits. The HUD reports terrain LOD and streaming statistics.

### `showcasevoxel` — Voxel Workshop

```bash
cmake --build build --target showcasevoxel
./build/showcasevoxel
```

Explore and edit a small generated voxel island. Use **WASD** and the **mouse** to fly/look, **Space/Ctrl** to move vertically, **1–4** to choose a block, **X** to mine, and **Z** to place; **Escape** exits. The world demonstrates chunk meshing, propagated block light, and DDA raycasting.

## 2D Games and Architecture

For a larger complete game, continue with [`exroguelike`](../src/examples/exroguelike/exroguelike.cpp): procedural worlds and dungeons, entity management, field of view, input, lighting, and an event-driven game loop. Its [architecture guide](exroguelike_guide.md) explains how the game, ECS, world generation, and rendering layers stay decoupled.

Then try:

- [`execs`](../src/examples/execs/execs.cpp) for interactive entity-component systems, archetypes, and parallel iteration ([guide](ecs.md)).
- [`exisometric`](../src/examples/exisometric/exisometric.cpp) for isometric rendering, elevation, mouse picking, and lighting.
- [`extown`](../src/examples/extown/extown.cpp) for a 55k-triangle town and airfield built from `props3d.h` parts with baked instances, shadows and night lamps.
- [`excontrols`](../src/examples/excontrols/excontrols.cpp) and [`excockpit`](../src/examples/excockpit/excockpit.cpp) for mouse-operated cockpit and factory control panels with glowing LEDs, in 2D and as 3D geometry.
- [`exxeno`](../src/examples/exxeno/exxeno.cpp) for a lit 2D shooter combining tile-map decals, `LightingPass`, particles and synthesised audio.
- [`exinvaders`](../src/examples/exinvaders/exinvaders.cpp) for a voxel Space Invaders with bloom and 3D debris from `RigidBoxWorld3D` ([guide](examples/exinvaders.md)).
- [`exdefender`](../src/examples/exdefender/exdefender.cpp) for direct four-way wraparound combat, rescues, voxel aliens, bloom and 3D debris ([guide](examples/exdefender.md)).
- [`extilemap`](../src/examples/extilemap/extilemap.cpp) and [`exphysics`](../src/examples/exphysics/exphysics.cpp) for tile maps and Box2D as focused building blocks.
- [`exaudio`](../src/examples/exaudio/exaudio.cpp), [`exparticles`](../src/examples/exparticles/exparticles.cpp), and [`exlighting`](../src/examples/exlighting/exlighting.cpp) for sound, particles, and dynamic 2D light/shadow systems.

## Terrain, Flight, and Space

Flat terrain also carries an `sl::Ocean` sea with shoaling waves and shoreline foam (`O` key). For spherical terrain and a larger set of rendering controls, try [`exterrain`](../src/examples/exterrain/exterrain.cpp) with its `planet` argument. See the [terrain](terrain.md) and [world flavour](world_flavour.md) guides for feature details.

Continue with [`exflight`](../src/examples/exflight/exflight.cpp) for aircraft lift, stalls, control surfaces, and landing gear, then [`exorbit`](../src/examples/exorbit/exorbit.cpp) for spacecraft, RCS, orbital prediction, and time warp. Shared APIs and conventions are documented in [Flight & Orbital Dynamics](flight.md).

## Voxel Worlds and Simulation

For a richer first-person voxel game with atlas textures and block editing, continue with [`exvoxel`](../src/examples/exvoxel/exvoxel.cpp). [`exfluid3d`](../src/examples/exfluid3d/exfluid3d.cpp) uses a voxel world as the simulation surface for cellular-automata fluids, fire, and sand. For the 2D fluid sandbox and interactive scenarios, see [`excafluid`](../src/examples/excafluid/excafluid.cpp) and its [guide](excafluid_guide.md).

## Graphics and Creative Tools

- [`exgpuparticles`](../src/examples/exgpuparticles/exgpuparticles.cpp) combines GPU compute, force fields, explosions, and glow ([guide](exgpuparticles_guide.md)).
- [`expostprocess`](../src/examples/expostprocess/expostprocess.cpp) explores screen-space effects and effect chaining; [`exnormalmap`](../src/examples/exnormalmap/exnormalmap.cpp) and [`exnormalmap3d`](../src/examples/exnormalmap3d/exnormalmap3d.cpp) demonstrate normal-mapped lighting.
- [`slpaint`](../src/examples/slpaint/slpaint.cpp) and [`exspriteeditor`](../src/examples/exspriteeditor/exspriteeditor.cpp) are usable art tools, not just API snippets.
- [`exgui`](../src/examples/exgui/exgui.cpp), [`exluaconsole`](../src/examples/exluaconsole/exluaconsole.cpp), and [`exconfig`](../src/examples/exconfig/exconfig.cpp) cover UI integration, interactive scripting, and configuration workflows.

Each executable can be built and launched on its own. The project keeps demos focused and composable: use the catalog above to combine systems for your application rather than treating one sample game as the full framework.
