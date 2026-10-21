#include "sl.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr int grid = 80;
    constexpr int sprite_count = 1500;

    sl::Bitmap *make_arrow()
    {
        sl::Bitmap *bitmap = sl::create_bitmap(16, 16);
        if (!bitmap)
            return nullptr;
        sl::clear_to_colour(bitmap, {0, 0, 0, 0});
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                if (x >= y / 2 && x <= 15 - y / 2)
                    sl::putpixel(bitmap, x, y, {255, 255, 255, 255});
        return bitmap;
    }

    // Per-cube height and tint are a function of position and time, shared by every mode.
    float wave(int x, int z, float time)
    {
        return std::sin(x * 0.25f + time * 1.5f) * std::cos(z * 0.25f + time) * 1.2f;
    }

    sl::Colour cube_tint(int x, int z)
    {
        return {static_cast<Uint8>(120 + 135 * x / grid), static_cast<Uint8>(120 + 135 * z / grid), 220, 255};
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 1000, 640))
        return -1;

    sl::Surface3D *surface = sl::create_surface_3d(940, 520, 4);
    sl::Mesh *cube = sl::create_cube_mesh(0.7f);
    sl::Bitmap *arrow = make_arrow();
    if (!surface || !cube || !arrow)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend.\n");
        sl::shutdown();
        return -1;
    }

    // The persistent buffer holds a static copy of the field, so it costs nothing per frame.
    std::vector<sl::InstanceData3D> instances(static_cast<std::size_t>(grid) * grid);
    sl::InstanceBuffer *buffer = sl::create_instance_buffer(instances.size());
    {
        std::size_t index = 0;
        for (int z = 0; z < grid; ++z)
            for (int x = 0; x < grid; ++x, ++index)
            {
                instances[index].model = sl::model_matrix({(x - grid / 2) * 1.0f, wave(x, z, 0.0f), (z - grid / 2) * 1.0f}, {0, 0, 0});
                instances[index].tint = cube_tint(x, z);
            }
        sl::update_instance_buffer(buffer, instances);
    }

    std::vector<sl::SpriteInstance> sprites(sprite_count);
    sl::Camera camera;
    camera.target = {0.0f, 0.0f, 0.0f};

    const char *mode_names[3] = {"One draw_mesh() per cube", "draw_mesh_instanced(vector): re-uploaded every frame",
                                 "draw_mesh_instanced(InstanceBuffer): uploaded once"};
    int mode = 1;
    bool running = true;
    float last_time = static_cast<float>(sl::time_ms()) * 0.001f;
    float time = 0.0f;
    float frame_ms = 16.0f;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space)
                mode = (mode + 1) % 3;
            sl::display_handle_event(event);
        }
        const float now = static_cast<float>(sl::time_ms()) * 0.001f;
        const float delta = now - last_time;
        last_time = now;
        time += delta;
        frame_ms += (delta * 1000.0f - frame_ms) * 0.05f;

        camera.position = {std::sin(time * 0.15f) * 55.0f, 28.0f, std::cos(time * 0.15f) * 55.0f};
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({20, 24, 40});
            sl::set_light_3d({{-0.4f, -1.0f, -0.3f}, 0.7f});
            sl::Material material;
            if (mode == 0)
            {
                for (int z = 0; z < grid; ++z)
                    for (int x = 0; x < grid; ++x)
                    {
                        material.tint = cube_tint(x, z);
                        sl::draw_mesh(cube, sl::model_matrix({(x - grid / 2) * 1.0f, wave(x, z, time), (z - grid / 2) * 1.0f}, {0, 0, 0}), material);
                    }
            }
            else if (mode == 1)
            {
                std::size_t index = 0;
                for (int z = 0; z < grid; ++z)
                    for (int x = 0; x < grid; ++x, ++index)
                        instances[index].model = sl::model_matrix({(x - grid / 2) * 1.0f, wave(x, z, time), (z - grid / 2) * 1.0f}, {0, 0, 0});
                sl::draw_mesh_instanced(cube, instances, material);
            }
            else
            {
                sl::draw_mesh_instanced(cube, buffer, material);
            }
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {30, 30, 30});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 30, 60);

        // 2D: a swarm of rotating, tinted sprites submitted as one batch.
        for (int i = 0; i < sprite_count; ++i)
        {
            const float angle = i * 0.37f + time * (0.4f + (i % 7) * 0.1f);
            const float radius = 30.0f + (i % 97) * 4.0f;
            sprites[i].x = 500.0f + std::cos(angle) * radius * 1.2f;
            sprites[i].y = 320.0f + std::sin(angle) * radius * 0.55f;
            sprites[i].rotation = angle * 57.2958f + 90.0f;
            sprites[i].scale_x = sprites[i].scale_y = 0.4f + (i % 5) * 0.15f;
            sprites[i].tint = {255, static_cast<Uint8>(120 + (i * 7) % 130), static_cast<Uint8>(60 + (i * 13) % 150), 180};
        }
        sl::draw_sprite_batch(arrow, sprites);

        sl::gprintf_center(24, {232, 236, 244}, "%d cubes: %s", grid * grid, mode_names[mode]);
        sl::gprintf_center(562, {232, 236, 244}, "%.1f ms/frame   Space switches mode, Escape exits", frame_ms);
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_instance_buffer(buffer);
    sl::destroy_mesh(cube);
    sl::destroy_bitmap(arrow);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
