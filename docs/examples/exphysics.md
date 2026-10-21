# exphysics

Box2D physics: a static floor and walls with falling boxes, circles and polygons, and contact polling.

![exphysics screenshot](img/exphysics.png)

## What it shows
- Creating a world with `create_physics_world` and stepping it each frame
- Static and dynamic bodies with box, circle and polygon fixtures
- Reading body position and angle to draw shapes
- Counting contact events with `poll_physics_contacts`

## Key code

In `exphysics.cpp`, a world is created with gravity, then static and dynamic bodies get box, circle and polygon fixtures.

```cpp
sl::PhysicsWorld *world = sl::create_physics_world({0.0f, 980.0f});
sl::PhysicsBody *floor = sl::create_physics_body(world, sl::BodyType::static_body, {400.0f, 560.0f});
sl::PhysicsBody *box = sl::create_physics_body(world, sl::BodyType::dynamic_body, {400.0f, 120.0f});
// ...
!sl::add_box_fixture(floor, 700.0f, 32.0f) ||
!sl::add_box_fixture(box, 64.0f, 64.0f, 1.0f, 0.4f, 0.35f) ||
!sl::add_circle_fixture(ball, 24.0f, 1.0f, 0.3f, 0.6f) ||
!sl::add_polygon_fixture(triangle, triangle_vertices, 1.0f, 0.3f, 0.35f))
```

Each frame the world is stepped, contact events are polled, and body positions and angles are read back for drawing.

```cpp
sl::step_physics_world(world, std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f));
const std::vector<sl::PhysicsContact> contacts = sl::poll_physics_contacts(world);
const sl::Vec2 box_position = sl::physics_body_position(box);
const sl::Vec2 ball_position = sl::physics_body_position(ball);
const sl::Vec2 triangle_position = sl::physics_body_position(triangle);
const float triangle_angle = sl::physics_body_angle(triangle);
```

The contact count is displayed as live text.

```cpp
sl::gprintf_center(56, {170, 190, 210}, "Contact events this frame: %d", static_cast<int>(contacts.size()));
```

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
