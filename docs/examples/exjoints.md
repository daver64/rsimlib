# exjoints

Physics joints: a revolute bridge, a spring pendulum and a wheel-jointed car, all draggable with the mouse.

![exjoints screenshot](img/exjoints.png)

## What it shows
- Revolute, distance (spring) and wheel joints
- Motor control with `set_joint_motor`
- Mouse dragging with `create_mouse_joint` and `physics_query_point`
- Debug drawing with `debug_draw_physics`

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
