#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr int tile = 64;

    // A rounded "bump" tile and its tangent-space normal map, generated procedurally.
    void make_bump_tile(sl::Bitmap *colour, sl::Bitmap *normal)
    {
        const float centre = (tile - 1) * 0.5f;
        const float radius = tile * 0.42f;
        for (int y = 0; y < tile; ++y)
        {
            for (int x = 0; x < tile; ++x)
            {
                const float dx = (x - centre) / radius;
                const float dy = (y - centre) / radius;
                const float d2 = dx * dx + dy * dy;
                if (d2 >= 1.0f)
                {
                    sl::putpixel(colour, x, y, {70, 60, 55, 255});
                    sl::putpixel(normal, x, y, {128, 128, 255, 255});
                    continue;
                }
                // Dome surface: n = (dx, -dy_up, sqrt(1 - d2)); green points up the image.
                const float nz = std::sqrt(1.0f - d2);
                const float nx = dx;
                const float ny_up = -dy;
                sl::putpixel(colour, x, y, {200, 170, 120, 255});
                sl::putpixel(normal, x, y,
                             {static_cast<std::uint8_t>((nx * 0.5f + 0.5f) * 255.0f),
                              static_cast<std::uint8_t>((ny_up * 0.5f + 0.5f) * 255.0f),
                              static_cast<std::uint8_t>((nz * 0.5f + 0.5f) * 255.0f), 255});
            }
        }
    }
} // namespace

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Bitmap *brick = sl::create_bitmap(tile, tile);
    sl::Bitmap *brick_normal = sl::create_bitmap(tile, tile);
    sl::Bitmap *scene = sl::create_render_target(800, 600);
    sl::Bitmap *normals = sl::create_render_target(800, 600);
    sl::LightingPass lighting;
    if (!brick || !brick_normal || !scene || !normals || !lighting.initialise())
    {
        std::fprintf(stderr, "normal map example init failed: %s\n", lighting.error().c_str());
        sl::shutdown();
        return -1;
    }
    make_bump_tile(brick, brick_normal);
    sl::set_normal_map(brick, brick_normal);
    lighting.set_ambient(0.12f);
    lighting.set_light_height(48.0f);

    bool use_normals = true;
    sl::set_fps(60);
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space)
            {
                use_normals = !use_normals;
            }
        }

        const auto draw_tiles = [&]()
        {
            for (int y = 0; y < 600; y += tile)
            {
                for (int x = 0; x < 800; x += tile)
                {
                    sl::draw_sprite(brick, static_cast<float>(x), static_cast<float>(y));
                }
            }
        };

        sl::begin_render_target(scene);
        sl::clear_render_target({40, 40, 46});
        draw_tiles();
        sl::end_render_target();

        if (sl::begin_normal_pass(normals))
        {
            draw_tiles();
            sl::end_normal_pass();
        }

        sl::Light light;
        light.x = static_cast<float>(sl::mouse_x());
        light.y = static_cast<float>(sl::mouse_y());
        light.radius = 300.0f;
        light.intensity = 1.6f;
        light.colour = {255, 210, 150};

        if (use_normals)
        {
            lighting.apply(scene, normals, {light});
        }
        else
        {
            lighting.apply(scene, std::vector<sl::Light>{light});
        }
        sl::gprintf_center(24, {0, 255, 0}, "Normal mapping %s - Space toggles, move the mouse, Esc exits",
                           use_normals ? "ON" : "OFF");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }
    sl::wait_for_graphics();
    lighting.shutdown();
    sl::destroy_bitmap(normals);
    sl::destroy_bitmap(scene);
    sl::destroy_bitmap(brick_normal);
    sl::destroy_bitmap(brick);
    sl::shutdown();
    return 0;
}
