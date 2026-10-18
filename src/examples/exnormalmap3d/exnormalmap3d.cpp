#include "sl.h"

#include <cmath>
#include <cstdio>

namespace
{
    // A grid of rounded studs, as a colour map and a matching tangent-space normal map.
    void make_studs(int size, int cells, sl::Bitmap *&colour, sl::Bitmap *&normal)
    {
        colour = sl::create_bitmap(size, size);
        normal = sl::create_bitmap(size, size);
        const float cell = static_cast<float>(size) / cells;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                // Position within the cell, -1..1 from its centre; image y grows downward.
                const float dx = (std::fmod(x + 0.5f, cell) / cell) * 2.0f - 1.0f;
                const float dy = 1.0f - (std::fmod(y + 0.5f, cell) / cell) * 2.0f;
                const float d2 = dx * dx + dy * dy;
                float nx = 0.0f, ny = 0.0f, nz = 1.0f;
                if (d2 < 0.64f)
                {
                    const float stud = 0.8f;
                    nx = dx / stud;
                    ny = dy / stud;
                    nz = std::sqrt(std::max(0.05f, 1.0f - nx * nx * 0.9f - ny * ny * 0.9f));
                    const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
                    nx /= length;
                    ny /= length;
                    nz /= length;
                }
                auto channel = [](float v) { return static_cast<std::uint8_t>((v * 0.5f + 0.5f) * 255.0f); };
                sl::putpixel(colour, x, y, {220, 190, 120, 255});
                sl::putpixel(normal, x, y, {channel(nx), channel(ny), channel(nz), 255});
            }
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
        return -1;

    sl::Surface3D *surface = sl::create_surface_3d(640, 480, 4);
    sl::Mesh *cube = sl::create_cube_mesh(1.2f);
    sl::Mesh *sphere = sl::create_sphere_mesh(0.7f, 48, 24);
    sl::Bitmap *colour = nullptr, *normal = nullptr;
    make_studs(256, 8, colour, normal);
    if (!surface || !cube || !sphere || !colour || !normal)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend.\n");
        sl::shutdown();
        return -1;
    }

    sl::Camera camera;
    camera.position = {0.0f, 1.2f, 4.2f};
    camera.target = {0.0f, 0.0f, 0.0f};

    bool use_normals = true;
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space)
                use_normals = !use_normals;
            sl::display_handle_event(event);
        }

        const float time = static_cast<float>(sl::time_ms()) * 0.001f;
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({15, 18, 30});
            // The light circles the scene so the studs' shading visibly moves.
            sl::set_light_3d({{-std::cos(time), -0.5f, -std::sin(time)}, 0.12f});
            sl::Material material;
            material.texture = colour;
            material.normal_map = use_normals ? normal : nullptr;
            sl::draw_mesh(cube, sl::model_matrix({-1.1f, 0, 0}, {0.4f, 0.6f, 0}), material);
            sl::draw_mesh(sphere, sl::model_matrix({1.1f, 0, 0}, {0, time * 0.3f, 0}), material);
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {40, 40, 40});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 80, 60);
        sl::gprintf_center(24, {232, 236, 244}, "3D normal mapping: %s (Space toggles, Escape exits)",
                           use_normals ? "on" : "off");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(cube);
    sl::destroy_mesh(sphere);
    sl::destroy_bitmap(colour);
    sl::destroy_bitmap(normal);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
