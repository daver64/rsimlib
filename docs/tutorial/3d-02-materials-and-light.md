# Lesson 2 (3D): Materials, light and transparency

[← Tutorial index](README.md) · Previous: [Your first 3D scene](3d-01-first-scene.md)

**Goal:** texture a mesh, add sky and fog, and draw see-through objects correctly.

## The idea

- A `Material` bundles a **tint** colour, whether it's **lit**, an optional **texture** and more. A texture is an ordinary `Bitmap` stretched over the mesh using the mesh's *UV* coordinates (0–1 positions across the image).
- `set_environment_3d` describes the world around the camera: sun direction, sky colours and **fog** (distant things fade into haze, which hides the far edge of your world). `draw_sky_3d()` paints the sky behind everything.
- **Transparency** is set with `material.blend`: `MaterialBlend::alpha` mixes the mesh with what's behind it using the tint's alpha; `additive` adds light (glows, fire).
- Transparent meshes test against depth but don't write it, and the library does **not sort them**. So draw in this order: all opaque meshes first, then transparent ones **farthest first**. Get this wrong and glass shows the wrong things through it.

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
    sl::Bitmap *grass = sl::load_bitmap("assets/textures/grass.png");
    sl::Bitmap *crate_texture = sl::load_bitmap("assets/textures/wall.png");
    sl::Mesh *ground = sl::create_plane_mesh(60.0f, 60.0f);
    sl::Mesh *cube = sl::create_cube_mesh(1.0f);
    sl::Mesh *pane = sl::create_cube_mesh(1.0f, {120, 200, 255});
    if (!surface || !grass || !crate_texture || !ground || !cube || !pane)
    {
        return -1;
    }

    sl::Environment3D sky; // defaults: a blue sky and sun
    sky.fog_enabled = true;
    sky.fog_density = 0.02f;
    sl::set_light_3d({sky.sun_direction, 0.3f});

    sl::Material ground_material;
    ground_material.texture = grass;
    sl::Material crate_material;
    crate_material.texture = crate_texture;
    sl::Material glass; // see-through, blue
    glass.tint = {120, 200, 255, 110};
    glass.blend = sl::MaterialBlend::alpha;

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
        sl::Camera camera;
        camera.position = {0.0f, 2.2f, 7.0f};
        camera.target = {0.0f, 0.8f, 0.0f};
        sky.camera_altitude = camera.position.y;
        sky.camera_position = {camera.position.x, camera.position.y, camera.position.z};
        sky.time = time;
        sl::set_environment_3d(sky);

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({0, 0, 0});
            sl::draw_sky_3d();

            // 1. Opaque things first.
            sl::draw_mesh(ground, sl::model_matrix({0, 0, 0}, {0, 0, 0}), ground_material);
            sl::draw_mesh(cube, sl::model_matrix({0.0f, 0.5f, -4.0f}, {0.0f, time * 0.5f, 0.0f}), crate_material);
            sl::draw_mesh(cube, sl::model_matrix({-2.5f, 0.5f, -12.0f}, {0.0f, time * 0.5f, 0.0f}), crate_material);

            // 2. Transparent things last, farthest first.
            sl::draw_mesh(pane, sl::model_matrix({0.0f, 1.0f, -1.0f}, {0, 0, 0}, {3.0f, 2.0f, 0.1f}), glass);
            sl::draw_mesh(pane, sl::model_matrix({0.0f, 1.0f, 1.5f}, {0, 0, 0}, {3.0f, 2.0f, 0.1f}), glass);
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0.0f, 0.0f);
        sl::gprintf(10, 10, {255, 255, 255}, "Texture, sky, fog and two glass panes. Escape quits.");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(ground);
    sl::destroy_mesh(cube);
    sl::destroy_mesh(pane);
    sl::destroy_bitmap(grass);
    sl::destroy_bitmap(crate_texture);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
```

## What just happened

- Fog is why the far crate looks hazier than the near one. `fog_density` is how quickly that happens: raise it for a murkier world, lower it for a clear day.
- The glass panes use a tint alpha of 110 out of 255. The far pane (z = −1) is drawn before the near one (z = 1.5), because the camera is at positive z looking toward negative z.
- Because the crate is opaque and drawn before the glass, you see it through both panes. Drawing it afterwards would erase the glass where they overlap.
- Texture file loading is the same `load_bitmap` as for 2D sprites.

## Try it

1. Make the glass additive (`MaterialBlend::additive`) and set `glass.fog = false`, then look at how it glows rather than tints.
2. Set `glass.lit = false` to remove the light shading on the panes.
3. Turn off fog (`sky.fog_enabled = false`) and compare the horizon.
4. Add a third pane and work out where it belongs in the draw order.

## Go further

- Examples: [`ex3d_surface`](../examples/ex3d_surface.md), [`exclouds`](../examples/exclouds.md), [`exprecip3d`](../examples/exprecip3d.md). API: [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh), [World Flavour](../world_flavour.md).
- Next: [Models and a HUD](3d-03-models-and-hud.md).
