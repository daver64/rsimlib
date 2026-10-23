# Lesson 5 (3D): Local lights and shadows

[← Tutorial index](README.md) · Previous: [Many objects and threads](3d-04-many-objects-and-threads.md)

**Goal:** light a night scene with a moving point light and a spotlight, and make the sun, the point light and the spotlight cast shadows.

## The idea

- The scene has one **directional light** (the sun or moon, `set_light_3d`) plus up to 16 **local lights** (`set_local_lights_3d`). A `LocalLight3D` is a **point light** by default. Give it an `outer_angle` (degrees) and a `direction` and it becomes a **spotlight**.
- Local lights are measured in world units: `range` is where the light fades to exactly nothing, and `intensity` is a brightness multiplier. Bigger rooms need bigger intensities.
- Only **lit** materials respond to lights. Unlit materials (such as the glowing bulb below) ignore them.
- Shadows are opt-in and are **rendered ahead of the main pass** from the light's point of view, by drawing the casters again:
  - **Sun shadows**: a `ShadowMap3D`, filled between `begin_shadow_pass_3d(map, centre, radius)` and `end_shadow_pass_3d()`, then enabled with `set_shadow_map_3d(map)`. The map covers a square of side `2 * radius` around `centre`.
  - **Local shadows**: a `LocalShadowMap3D`. Set `cast_shadow = true` on a light (up to 4 lights), call `render_local_shadows_3d(map, draw_casters)`, then enable it with `set_local_shadow_map_3d(map)`. Your function is called once per face: one for a spotlight, six for a point light.
- Shadow passes cannot happen between `begin_surface_3d()` and `end_surface_3d()`, and only opaque, lit `draw_mesh` calls cast shadows.
- A lit mesh only receives shadows if you draw it into the main surface *after* enabling the maps. Do the passes first, every frame, because things move.

## The program

```cpp
#include "sl.h"

#include <cmath>
#include <vector>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    sl::set_fps(60);

    sl::Surface3D *surface = sl::create_surface_3d(800, 600, 4);
    sl::Mesh *ground = sl::create_plane_mesh(40.0f, 40.0f, {150, 150, 155});
    sl::Mesh *cube = sl::create_cube_mesh(1.0f);
    sl::Mesh *sphere = sl::create_sphere_mesh(1.0f);
    sl::ShadowMap3D *sun_shadows = sl::create_shadow_map_3d(2048);
    sl::LocalShadowMap3D *light_shadows = sl::create_local_shadow_map_3d(512, 2);
    if (!surface || !ground || !cube || !sphere || !sun_shadows || !light_shadows)
    {
        return -1;
    }

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

        // A dim moon, a point light circling the pillars and a spotlight sweeping the ground.
        sl::Light3D moon;
        moon.direction = {-0.4f, -1.0f, -0.5f};
        moon.colour = {0.5f, 0.58f, 0.8f};
        moon.ambient = 0.08f;
        sl::set_light_3d(moon);

        sl::LocalLight3D lamp;
        lamp.position = {std::cos(time) * 4.0f, 1.6f, std::sin(time) * 4.0f};
        lamp.colour = {1.0f, 0.7f, 0.4f};
        lamp.intensity = 12.0f;
        lamp.range = 14.0f;
        lamp.cast_shadow = true;

        sl::LocalLight3D spot;
        spot.position = {0.0f, 9.0f, 0.0f};
        spot.direction = glm::normalize(glm::vec3(std::cos(time * 0.4f) * 5.0f, -9.0f, std::sin(time * 0.4f) * 5.0f));
        spot.colour = {0.9f, 0.95f, 1.0f};
        spot.intensity = 150.0f;
        spot.range = 30.0f;
        spot.inner_angle = 10.0f;
        spot.outer_angle = 25.0f;
        spot.cast_shadow = true;
        sl::set_local_lights_3d({lamp, spot});

        // Everything that casts a shadow, drawn the same way in every pass.
        auto draw_scene = [&]()
        {
            sl::Material material;
            material.tint = {200, 190, 175, 255};
            sl::draw_mesh(ground, sl::model_matrix({0, 0, 0}, {0, 0, 0}));
            for (int i = 0; i < 4; ++i)
            {
                const float angle = static_cast<float>(i) * 1.5707963f + 0.4f;
                sl::draw_mesh(cube, sl::model_matrix({std::cos(angle) * 7.0f, 2.0f, std::sin(angle) * 7.0f}, {0, 0, 0}, {1.2f, 4.0f, 1.2f}), material);
            }
            sl::draw_mesh(sphere, sl::model_matrix({0.0f, 1.0f, 0.0f}, {0, 0, 0}), material);
        };

        // Shadow passes come first, outside begin_surface_3d().
        sl::set_local_shadow_map_3d(nullptr);
        if (sl::render_local_shadows_3d(light_shadows, draw_scene))
            sl::set_local_shadow_map_3d(light_shadows);
        sl::set_shadow_map_3d(nullptr);
        if (sl::begin_shadow_pass_3d(sun_shadows, {0.0f, 0.0f, 0.0f}, 14.0f))
        {
            draw_scene();
            sl::end_shadow_pass_3d();
            sl::set_shadow_map_3d(sun_shadows);
        }

        sl::Camera camera;
        camera.position = {0.0f, 9.0f, 16.0f};
        camera.target = {0.0f, 1.0f, 0.0f};
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({6, 8, 16});
            draw_scene();

            // The lamp itself: unlit, so lights and shadows leave it alone.
            sl::Material bulb;
            bulb.lit = false;
            bulb.tint = {255, 190, 120, 255};
            sl::draw_mesh(sphere, sl::model_matrix(lamp.position, {0, 0, 0}, {0.2f, 0.2f, 0.2f}), bulb);
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0.0f, 0.0f);
        sl::gprintf(10, 10, {255, 255, 255}, "Moon, point light and spotlight, all casting shadows. Escape quits.");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::set_local_shadow_map_3d(nullptr);
    sl::set_shadow_map_3d(nullptr);
    sl::set_local_lights_3d({});
    sl::destroy_local_shadow_map_3d(light_shadows);
    sl::destroy_shadow_map_3d(sun_shadows);
    sl::destroy_mesh(ground);
    sl::destroy_mesh(cube);
    sl::destroy_mesh(sphere);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
```

## What just happened

- `draw_scene` is a lambda so the same code draws the casters into the sun map, into each local light face, and finally into the screen surface. Keep the three in step or shadows will not match the objects.
- The lamp and the spotlight both have `cast_shadow = true`, so they take shadow slots in the order they appear in the list. The map was created with room for 2 lights; extra shadow-casting lights beyond that simply do not cast.
- `render_local_shadows_3d` uses the lights from the latest `set_local_lights_3d` call. If you change the lights after rendering the shadows, the shadows no longer match; set them first, render second, draw third.
- Light is exactly zero at `range`, so lights far from the camera cost nothing and never leave a hard edge.
- The shadow window is centred on the origin with a radius of 14, so things beyond that have no sun shadow. Move `centre` with the camera or player for a larger world.
- Local shadows are available on OpenGL and Vulkan. On Direct3D 12 the lights work, but local shadows are not implemented yet and sun shadows are untested.

## Try it

1. Turn `lamp.cast_shadow` off and watch the pillar shadows disappear, then restore it.
2. Add a third light with `cast_shadow = true`. Raise `create_local_shadow_map_3d(512, 2)` to `(512, 3)` so it gets a slot.
3. Lower `moon.ambient` to 0.0 and raise the lamp `intensity` until the scene reads well again.
4. Change `inner_angle` and `outer_angle` to see how the spotlight edge softens or sharpens.
5. Pass `nullptr` to `set_shadow_map_3d` and compare the moon's shadows on and off.
6. Make the sphere bob up and down (it is drawn by `draw_scene`, so its shadow follows).

## Go further

- Examples: [`exlights3d`](../examples/exlights3d.md) (up to 16 lights and all three shadow types), [`exterrain`](../examples/exterrain.md) (a lantern lighting terrain). API: [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh).
- Related: [`exhdr`](../examples/exhdr.md) for bright lights with bloom and tone mapping.
- Next track: [Physics bodies](sim-01-physics-bodies.md).
