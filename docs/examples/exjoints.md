# exjoints

Physics joints: a revolute bridge, a spring pendulum and a wheel-jointed car, all draggable with the mouse.

![exjoints screenshot](img/exjoints.png)

## What it shows
- Revolute, distance (spring) and wheel joints
- Motor control with `set_joint_motor`
- Mouse dragging with `create_mouse_joint` and `physics_query_point`
- Debug drawing with `debug_draw_physics`

## Key code

In `exjoints.cpp`, revolute joints connect the bridge planks end to end.
```cpp
    const int planks = 8;
    const float plank_width = 44.0f, bridge_y = 250.0f, bridge_x = 60.0f;
    sl::PhysicsBody *left_post = sl::create_physics_body(world, sl::BodyType::static_body, {bridge_x - 10.0f, bridge_y});
    sl::add_box_fixture(left_post, 20.0f, 30.0f);
    sl::PhysicsBody *previous = left_post;
    for (int i = 0; i < planks; ++i)
    {
        const float x = bridge_x + plank_width * (i + 0.5f);
        sl::PhysicsBody *plank = sl::create_physics_body(world, sl::BodyType::dynamic_body, {x, bridge_y});
        sl::add_box_fixture(plank, plank_width - 2.0f, 10.0f, 1.0f, 0.5f);
        sl::create_revolute_joint(world, previous, plank, {bridge_x + plank_width * i, bridge_y});
        previous = plank;
    }
```

In `exjoints.cpp`, the pendulum uses a distance joint and each car wheel uses a motorized wheel joint.
```cpp
    sl::PhysicsBody *hook = sl::create_physics_body(world, sl::BodyType::static_body, {620.0f, 60.0f});
    sl::add_circle_fixture(hook, 6.0f);
    sl::PhysicsBody *bob = sl::create_physics_body(world, sl::BodyType::dynamic_body, {720.0f, 60.0f});
    sl::add_circle_fixture(bob, 22.0f, 1.0f, 0.3f, 0.2f);
    sl::create_distance_joint(world, hook, bob, {620.0f, 60.0f}, {720.0f, 60.0f}, 2.0f, 0.3f);
    // ...
        sl::PhysicsJoint *joint = sl::create_wheel_joint(world, chassis, wheel, {120.0f + offset, 505.0f}, {0.0f, 1.0f});
        sl::set_joint_motor(joint, 6.0f, 60000.0f);
        motors.push_back(joint);
```

In `exjoints.cpp`, `physics_query_point` finds a body under the cursor so a mouse joint can follow the pointer during a drag.
```cpp
        const sl::Vec2 mouse{static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y())};
        const bool held = (sl::mouse_buttons() & sl::MOUSE_BUTTON_LEFT_MASK) != 0;
        if (held && !grab)
        {
            for (sl::PhysicsBody *body : sl::physics_query_point(world, mouse))
            {
                if ((grab = sl::create_mouse_joint(world, body, mouse)))
                    break;
            }
        }
        else if (held && grab)
            sl::set_mouse_joint_target(grab, mouse);
        else if (!held && grab)
        {
            sl::destroy_physics_joint(grab);
            grab = nullptr;
        }
```

## Running
```bash
./build/exjoints
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Set `RSIMLIB_SCREENSHOT=<file.png>` to save a screenshot on an early frame.

## Controls
| Input | Action |
|-------|--------|
| Left mouse drag | Grab and drag a body |
| Escape | Exit |

## Key APIs
- `create_revolute_joint`, `create_distance_joint`, `create_wheel_joint`, `set_joint_motor` — [Box2D](../api_reference.md#box2d-physics-system-includephysicsh) · [`physics.h`](../../include/physics.h)
- `create_mouse_joint`, `set_mouse_joint_target`, `physics_query_point` — [Box2D](../api_reference.md#box2d-physics-system-includephysicsh) · [`physics.h`](../../include/physics.h)
- `debug_draw_physics` — [Box2D](../api_reference.md#box2d-physics-system-includephysicsh) · [`physics.h`](../../include/physics.h)

## Source
[src/examples/exjoints/exjoints.cpp](../../src/examples/exjoints/exjoints.cpp)

## See also
- [exphysics](exphysics.md)

See also the [Physics guide](../physics.md).
