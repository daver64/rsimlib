# Physics & Collision Integration

[← Back to README](../README.md)

`simlib` provides an opaque C++ wrapper around Box2D, as well as a lightweight entity simulation model and sandboxed Lua physics bindings.

---

## Box2D C++ Wrapper (`physics.h`)

`physics.h` wraps Box2D behind opaque handles. Coordinates use pixel space; the wrapper automatically converts via `physics_pixels_per_meter` (64 pixels per metre).

| Function | Description |
| --- | --- |
| `create_physics_world(gravity)` / `destroy_physics_world(world)` | Create or release a Box2D world. Gravity is in pixels/s². |
| `create_physics_body(world, type, position)` / `destroy_physics_body(body)` | Create or release static, kinematic, or dynamic bodies. |
| `add_box_fixture(body, width, height, density, friction, restitution)` | Add a box shape using pixel dimensions. |
| `add_circle_fixture(body, radius, density, friction, restitution)` | Add a circle shape using a pixel radius. |
| `add_polygon_fixture(body, vertices, density, friction, restitution)` | Add a convex polygon shape using up to 8 pixel-space vertices. |
| `step_physics_world(world, timeStep, velocityIterations, positionIterations)` | Advance the simulation. |
| `poll_physics_contacts(world)` | Return and clear begin/end contact events since the previous call. |
| `physics_body_position(body)` / `physics_body_angle(body)` | Read a body's transform in pixel coordinates and radians. |
| `set_physics_body_transform(body, position, angle)` | Set a body's pixel-space transform. |
| `set_physics_body_velocity(body, velocity)` | Set linear velocity in pixels per second. |
| `apply_physics_force(body, force)` | Apply a force at the body's centre of mass. |

### Contacts
`PhysicsContact` contains contact `type`, body handles, pixel-space contact `point`, and a unit `normal`.

---

## Lua Physics API

In Lua scripts, physics functions are exposed under the `physics` table using safe integer handles:

```lua
local world = physics.create_world(0, 980)
local floor = physics.create_body(world, "static", 400, 560)
local ball = physics.create_body(world, "dynamic", 400, 100)
physics.add_box(floor, 700, 32)
physics.add_circle(ball, 24, 1.0, 0.3, 0.6)

-- Step simulation:
physics.step(world, 1 / 60)
local position = physics.position(ball)
local events = physics.contacts(world)
physics.destroy_world(world) -- releases bodies automatically
```

---

## Simple Entity Physics (`entity.h` in `sltest`)

For lightweight games that do not need full rigid-body dynamics, `sltest` includes a simple particle/circle/AABB integrator:

```cpp
#include "entity.h"

std::vector<game::GameObject> objects;
objects.push_back(game::make_circle_object(sprite, 100.0f, 100.0f, /*radius*/16.0f, /*mass*/1.0f));

// Per-frame update:
game::physics_step(objects, dt_seconds);
game::resolve_collisions(objects);
game::constrain_to_screen(objects);
game::render_objects(objects);
```

---

[← Back to README](../README.md)
