# Lesson 3 (3D): Models and a HUD

[← Tutorial index](README.md) · Previous: [Materials, light and transparency](3d-02-materials-and-light.md)

**Goal:** load 3D models from files, fly the camera with the keyboard, and draw 2D text and shapes on top of the 3D view.

## The idea

- Real 3D art comes from modelling tools such as Blender. simlib loads Wavefront `.obj`/`.mtl`, `.gltf` and `.glb`. `sl::load_model(path)` loads one, with its materials and textures, and `sl::draw_model(model, matrix)` draws it. Destroy it with `sl::destroy_model`.
- Models are loaded once and drawn many times, each with its own matrix.
- A **HUD** (heads-up display) is 2D drawn over the 3D view. Since the 3D surface is just a bitmap, you can either draw 2D onto the screen *after* the surface, or onto the surface itself (shapes and text drawn with `sl::surface_3d_bitmap(surface)` as the target).
- To fly, keep the camera as a position plus a heading angle, and move along the direction it faces.

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
    sl::Mesh *floor = sl::create_plane_mesh(30.0f, 30.0f, {90, 100, 90});
    sl::Model *crate = sl::load_model("assets/models/crate.obj");
    sl::Model *gems = sl::load_model("assets/models/gem.gltf");
    if (!surface || !floor || !crate || !gems)
    {
        return -1;
    }
    sl::set_light_3d({{-0.4f, -1.0f, -0.6f}, 0.3f});

    glm::vec3 position{0.0f, 1.5f, 6.0f};
    float heading = 0.0f; // radians; 0 looks toward -z
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

        const float dt = static_cast<float>(sl::get_frame_time()) / 1000.0f;
        const float time = static_cast<float>(sl::time_ms()) * 0.001f;
        if (sl::key_down(sl::Event::Key::arrow_left))
            heading += 1.6f * dt;
        if (sl::key_down(sl::Event::Key::arrow_right))
            heading -= 1.6f * dt;
        const glm::vec3 forward{-std::sin(heading), 0.0f, -std::cos(heading)};
        if (sl::key_down(sl::Event::Key::arrow_up))
            position += forward * 4.0f * dt;
        if (sl::key_down(sl::Event::Key::arrow_down))
            position -= forward * 4.0f * dt;

        sl::Camera camera;
        camera.position = position;
        camera.target = position + forward;

        sl::Bitmap *hud = sl::surface_3d_bitmap(surface);
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({30, 40, 70});
            sl::draw_mesh(floor, sl::model_matrix({0, 0, 0}, {0, 0, 0}), {{255, 255, 255}, true});
            sl::draw_model(crate, sl::model_matrix({-2.0f, 0.0f, 0.0f}, {0.0f, time * 0.4f, 0.0f}));
            sl::draw_model(gems, sl::model_matrix({2.0f, 0.5f, -1.0f}, {0.0f, -time * 0.6f, 0.0f}));

            // HUD drawn straight into the 3D surface.
            sl::rectfill(hud, 10.0f, 10.0f, 330.0f, 56.0f, {10, 14, 30, 190});
            sl::gprintf(20, 18, {255, 255, 255}, "x %.1f  z %.1f", position.x, position.z);
            sl::gprintf(20, 36, {160, 200, 255}, "Arrows: turn and walk");
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(hud, 0.0f, 0.0f);
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_model(crate);
    sl::destroy_model(gems);
    sl::destroy_mesh(floor);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
```

## What just happened

- `forward` points where the camera faces on the ground plane. Turning changes `heading`; walking adds `forward * speed * dt` to the position. The camera's `target` is simply one step ahead.
- The HUD is drawn between `begin_surface_3d` and `end_surface_3d`, so it gets the same smoothing as the 3D and costs nothing extra. Drawing text after `end_surface_3d` onto `sl::screen` works too.
- A model can contain many meshes and animations; `draw_model` draws them all. See the model examples for animated and skinned models.

## Try it

1. Draw a ring of eight crates with a loop, each with its own position.
2. Add a crosshair: two short `sl::line` calls at the centre of the view.
3. Export a model from Blender as `.glb` and load it. The [Blender guide](../blender.md) covers the settings.

## Loading without a stall

Big models can hitch a frame. `sl::load_model_async(path)` decodes on a worker thread and uploads a little each `end_frame()`. Poll the handle: `auto h = sl::load_model_async(p);` then, once `h.ready()`, take `sl::Model *m = h.get();`. `h.failed()` and `h.error()` report problems. `sl::load_bitmap_async` works the same way.

## Go further

- Examples: [`exmodel3d`](../examples/exmodel3d.md), [`expbr`](../examples/expbr.md), [`exvoxel`](../examples/exvoxel.md). API: [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh), [3D Models & Async Loading](../api_reference.md#3d-models--async-loading-includemodel3dh-includeasync_loadh).
- Next: [Many objects and threads](3d-04-many-objects-and-threads.md), then the [Physics bodies](sim-01-physics-bodies.md) track, or a bigger demo in the [Showcase Guide](../showcases.md).
