# Lesson 1 (Simulation): Physics bodies

[← Tutorial index](README.md) · Previous: [Many objects and threads](3d-04-many-objects-and-threads.md)

**Goal:** drop boxes and balls onto a floor and watch them collide, using the 2D physics world.

## The idea

- A **physics world** holds bodies and steps them forward in time: `sl::create_physics_world(gravity)`. Gravity here is in pixels per second squared, so `{0, 980}` pulls down the screen (y grows downward).
- A **body** is a point with a position and motion. Make it `static_body` (never moves: floors, walls) or `dynamic_body` (moved by forces and collisions).
- A body has no shape until you add a **fixture**: `add_box_fixture` or `add_circle_fixture`, with density (mass per area), friction and restitution (bounciness, 0 = dead, 1 = perfectly bouncy).
- Each frame call `step_physics_world(world, dt)`, then read positions with `physics_body_position` and angles with `physics_body_angle`.
- `debug_draw_physics(world)` outlines every fixture, which is the quickest way to see what the simulation is doing.

## The program

```cpp
#include "sl.h"

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    sl::set_fps(60);

    sl::PhysicsWorld *world = sl::create_physics_world({0.0f, 980.0f});

    sl::PhysicsBody *floor = sl::create_physics_body(world, sl::BodyType::static_body, {400.0f, 580.0f});
    sl::add_box_fixture(floor, 780.0f, 20.0f, 0.0f, 0.6f, 0.0f);

    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;

            // Click to drop a box; hold shift for a ball.
            if (event.type() == sl::Event::Type::mouse_button_down)
            {
                const sl::Vec2 at{static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y())};
                sl::PhysicsBody *body = sl::create_physics_body(world, sl::BodyType::dynamic_body, at);
                if (sl::key_down(sl::Event::Key::left_shift))
                    sl::add_circle_fixture(body, 18.0f, 1.0f, 0.4f, 0.5f);
                else
                    sl::add_box_fixture(body, 36.0f, 36.0f, 1.0f, 0.4f, 0.2f);
            }
            sl::display_handle_event(event);
        }

        sl::step_physics_world(world, 1.0f / 60.0f);

        sl::clear_to_colour(sl::screen, {24, 28, 40});
        sl::debug_draw_physics(world);
        sl::gprintf(10, 10, {255, 255, 255}, "Click to drop a box, shift-click for a ball. Escape quits.");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_physics_world(world);
    sl::shutdown();
    return 0;
}
```

## What just happened

- The floor has density 0, which is fine for a static body. Dynamic bodies need density above zero or they have no mass.
- Stepping uses a fixed `1/60` second. Fixed steps keep a simulation repeatable; varying the step with frame rate makes it behave differently on slow machines.
- Boxes stack and slide because of friction; the circle rolls because circles have nothing to grip against.

## Try it

1. Raise the box restitution to 0.9 and watch them bounce.
2. Add walls: two more static bodies with tall `add_box_fixture` shapes at the left and right edges.
3. Draw your own sprites at `physics_body_position(body)` rotated by `physics_body_angle(body)` instead of using `debug_draw_physics`.
4. Call `sl::poll_physics_contacts(world)` after stepping to react to collisions, for example to play a sound.

## Go further

- Examples: [`exphysics`](../examples/exphysics.md), [`exjoints`](../examples/exjoints.md). API: [Physics](../api_reference.md#box2d-physics-system-includephysicsh).
- Next: [Joints](sim-02-joints.md).
