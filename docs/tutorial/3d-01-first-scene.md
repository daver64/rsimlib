# Lesson 1 (3D): Your first 3D scene

[← Tutorial index](README.md) · Previous track: [2D lessons](README.md)

**Goal:** draw spinning 3D shapes lit by a light, seen through a camera you can orbit.

## The idea

- 3D is drawn into an offscreen **surface** (a colour image plus a depth buffer that makes near things hide far things). Afterwards you draw that surface onto the 2D screen like any other bitmap, so 3D and 2D mix freely.
- Each frame: `begin_surface_3d(surface, camera)`, clear, draw meshes, `end_surface_3d()`.
- A **camera** has a `position`, a `target` it looks at and an `up` direction. Coordinates are right-handed: x right, y up, and z toward the viewer.
- A **mesh** is a shape's triangles. `create_cube_mesh`, `create_sphere_mesh` and `create_plane_mesh` make simple ones; later lessons load models from files.
- You place a mesh with a **model matrix**: `sl::model_matrix(position, rotation, scale)`, where rotation is angles in radians about x, y and z.
- A **material** says how a mesh looks. The simplest is `{colour, lit}`; `lit = true` shades it with the scene's directional light (`sl::set_light_3d`).
- The last argument of `create_surface_3d` is the number of **MSAA** samples, a smoothing of jagged edges. 4 is a good default.

## The program

```cpp
#include "sl.h"

#include <cmath>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    sl::set_fps(60);

    sl::Surface3D *surface = sl::create_surface_3d(800, 600, 4);
    sl::Mesh *cube = sl::create_cube_mesh(1.0f, {240, 120, 80});
    sl::Mesh *ball = sl::create_sphere_mesh(0.6f, 32, 16, {90, 170, 255});
    sl::Mesh *floor = sl::create_plane_mesh(10.0f, 10.0f, {110, 120, 110});
    if (!surface || !cube || !ball || !floor)
    {
        return -1;
    }

    sl::set_light_3d({{-0.4f, -1.0f, -0.6f}, 0.25f}); // direction, ambient

    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }

        const float time = static_cast<float>(sl::time_ms()) * 0.001f;

        // Orbit the camera around the origin.
        sl::Camera camera;
        camera.position = {6.0f * std::sin(time * 0.3f), 3.0f, 6.0f * std::cos(time * 0.3f)};
        camera.target = {0.0f, 0.5f, 0.0f};

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({20, 30, 60});
            sl::draw_mesh(floor, sl::model_matrix({0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}), {{255, 255, 255}, true});
            sl::draw_mesh(cube, sl::model_matrix({-1.2f, 0.0f, 0.0f}, {time, time * 1.3f, 0.0f}), {{255, 255, 255}, true});
            sl::draw_mesh(ball, sl::model_matrix({1.2f, 0.1f, 0.0f}, {0.0f, 0.0f, 0.0f}), {{255, 255, 255}, true});
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0.0f, 0.0f);
        sl::gprintf(10, 10, {255, 255, 255}, "A camera orbiting a lit scene. Escape quits.");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(cube);
    sl::destroy_mesh(ball);
    sl::destroy_mesh(floor);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
```

## What just happened

- The mesh colours you passed at creation are *vertex colours*; the material's tint `{255, 255, 255}` leaves them unchanged (it's multiplied in, so `{255, 0, 0}` would keep only the red).
- The camera is rebuilt every frame from the time, which is all "orbit" means: a point moving on a circle (`sin` and `cos`) while looking at the middle.
- The same mesh can be drawn many times with different matrices. Meshes are created once and drawn as often as you like.
- Meshes and surfaces use GPU memory, so destroy them before `shutdown()`.

## Try it

1. Add a second light direction, or make the ambient value 0.0 to see how dark the shadowed sides get.
2. Control the camera yourself: replace the orbit with `camera.position` driven by the arrow keys.
3. Change `field_of_view` on the camera (default 60 degrees) to zoom.

## Go further

- Examples: [`ex3d_surface`](../examples/ex3d_surface.md). API: [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh).
- Next: [Materials, light and transparency](3d-02-materials-and-light.md).
