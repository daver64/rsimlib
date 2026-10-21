# execs

A header-only archetype ECS (`recs.h`) driving thousands of simulated entities, rendered with simlib.

![execs screenshot](img/execs.png)

## What it shows
- Archetype-based entity storage with components and resources
- Wanderer, seeker and particle entities spawned interactively
- Serial, chunked and parallel iteration (`for_each_chunk`, `parallel_for_each`, `parallel_for_each_chunk`)
- Live archetype count and update and render timings
- ECS memory usage reporting

## Key code

In `execs.cpp`, resources are registered on the `recs::World` and wanderer and seeker entities are spawned into it at start-up:

```cpp
world.set_resource<GameTime>();

world.set_resource<MouseState>();

world.set_resource<Settings>();

world.set_resource<Stats>();
// ...
spawn_wanderers(
    world,
    500,
    350.0f,
    430.0f);

spawn_seekers(
    world,
    250,
    550.0f,
    430.0f);
```

The movement system switches between chunked and parallel iteration over `Position` and `Velocity` components:

```cpp
if (mode == 1)
{
    world.for_each_chunk<
        Position,
        Velocity>(
        [&](Position *positions,
            Velocity *velocities,
            size_t count)
        {
            for (
                size_t i = 0;
                i < count;
                ++i)
            {
                positions[i].x +=
                    velocities[i].x *
                    delta;
```

```cpp
world.parallel_for_each<
    Position,
    Velocity>(
    [&](Position &position,
        Velocity &velocity)
    {
        position.x +=
            velocity.x *
            delta;

        position.y +=
            velocity.y *
            delta;
    });
```

## Running
```bash
./build/execs
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Left / right mouse | Spawn wanderers / seekers |
| 1 / 2 / 3 | Spawn 100 wanderers / 100 seekers / 500 particles |
| C | Iterate with `for_each_chunk` |
| P | Iterate with `parallel_for_each` |
| K | Iterate with `parallel_for_each_chunk` |
| R | Reset |
| Escape | Exit |

## Key APIs
- RECS entities, components, archetypes and iteration (`src/examples/execs/recs.h`)
- `circlefill`, `line`, `gprintf` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `get_frame_time`, `mouse_buttons` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)

## Source
[src/examples/execs/execs.cpp](../../src/examples/execs/execs.cpp)
[src/examples/execs/recs.h](../../src/examples/execs/recs.h)

## See also
- [expathfinding](expathfinding.md)
- [exparticles](exparticles.md)

See also the [ECS guide](../ecs.md).
