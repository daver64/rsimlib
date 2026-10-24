# Physics & Collision Integration

[← Back to README](../README.md)

`simlib` provides an opaque C++ wrapper around Box2D and sandboxed Lua physics bindings.

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

## 3D Rigid Boxes (`physics3d.h`)

Box2D is 2D only. `RigidBoxWorld3D` simulates oriented boxes in 3D under gravity, with a floor plane and static boxes, impulse contacts that include rotation and friction, lifetimes and an explosion helper. It is meant for debris and effects (bodies do not collide with each other); see the [API reference](api_reference.md) and the `exinvaders` example.

---

## Lua Physics API

In Lua scripts, physics functions are exposed under the `physics` table using safe integer handles:

```lua
local world = physics.create_world(0, 980)
local floor = physics.create_body(world, "static", 400, 560)
local ball = physics.create_body(world, "dynamic", 400, 100)
physics.add_box(floor, 700, 32)
physics.add_circle(ball, 24, 1.0, 0.3, 0.6)
physics.add_polygon(ball, {{x = -10, y = -10}, {x = 10, y = -10}, {x = 0, y = 12}}) -- convex, optional density/friction/restitution
physics.set_velocity(ball, 120, -300)

-- Step simulation:
physics.step(world, 1 / 60)
local position = physics.position(ball)
local events = physics.contacts(world)
physics.destroy_body(ball)
physics.destroy_world(world) -- releases bodies automatically
```

---

## 3D Flight & Orbital Dynamics

For 3D rigid-body simulation (spacecraft, aircraft) see [Flight & Orbital Dynamics](flight.md) and `flight.h`.

---
[← Back to README](../README.md)
