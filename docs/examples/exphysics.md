# exphysics

Box2D physics: a static floor and walls with falling boxes, circles and polygons, and contact polling.

![exphysics screenshot](img/exphysics.png)

## What it shows
- Creating a world with `create_physics_world` and stepping it each frame
- Static and dynamic bodies with box, circle and polygon fixtures
- Reading body position and angle to draw shapes
- Counting contact events with `poll_physics_contacts`

## Running
```bash
./build/exphysics
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Escape, or closing the window | Exit |

## Key APIs
- `create_physics_world`, `step_physics_world`, `destroy_physics_world` — [Box2D](../api_reference.md#box2d-physics-system-includephysicsh) · [`physics.h`](../../include/physics.h)
- `create_physics_body`, `add_box_fixture`, `add_circle_fixture`, `add_polygon_fixture` — [Box2D](../api_reference.md#box2d-physics-system-includephysicsh) · [`physics.h`](../../include/physics.h)
- `physics_body_position`, `physics_body_angle`, `poll_physics_contacts` — [Box2D](../api_reference.md#box2d-physics-system-includephysicsh) · [`physics.h`](../../include/physics.h)

## Source
[src/examples/exphysics/exphysics.cpp](../../src/examples/exphysics/exphysics.cpp)

## See also
- [exjoints](exjoints.md)
- [execs](execs.md)

See also the [Physics guide](../physics.md).
