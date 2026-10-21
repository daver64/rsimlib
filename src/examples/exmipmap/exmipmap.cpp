#include "sl.h"

#include <cmath>
#include <cstdio>

namespace
{
    // Checkerboard with 1px grid lines: shimmers badly when minified without mipmaps.
    sl::Bitmap *make_checker(int size, bool mipmaps, float anisotropy)
    {
        sl::Bitmap *bitmap = sl::create_bitmap(size, size);
        if (!bitmap)
            return nullptr;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const bool light = ((x / 16) + (y / 16)) & 1;
                const bool line = (x % 16 == 0) || (y % 16 == 0);
                sl::Colour colour = light ? sl::Colour{230, 230, 235, 255} : sl::Colour{40, 60, 110, 255};
                if (line)
                    colour = {255, 160, 30, 255};
                sl::putpixel(bitmap, x, y, colour);
            }
        // These are read when the GPU texture is first created, so set them before the first draw.
        bitmap->smooth = true;
        bitmap->mipmaps = mipmaps;
        bitmap->anisotropy = anisotropy;
        return bitmap;
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 600))
        return -1;

    sl::Surface3D *surface = sl::create_surface_3d(900, 480, 4);
    sl::Mesh *plane = sl::create_plane_mesh(2.0f, 60.0f);
    // One texture per mode, since the sampling state is fixed per texture.
    sl::Bitmap *textures[3] = {make_checker(512, false, 1.0f), make_checker(512, true, 1.0f),
                               make_checker(512, true, 16.0f)};
    if (!surface || !plane || !textures[0] || !textures[1] || !textures[2])
    {
        std::fprintf(stderr, "3D surface unavailable on this backend.\n");
        sl::shutdown();
        return -1;
    }

    sl::Camera camera;
    camera.target = {0.0f, 0.0f, -30.0f};

    bool running = true;
    bool paused = false;
    float phase = 0.0f;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space)
                paused = !paused;
            sl::display_handle_event(event);
        }
        if (!paused)
            phase += 0.01f;

        // Sway sideways so the aliasing in the unfiltered plane visibly crawls.
        camera.position = {std::sin(phase) * 1.5f, 0.6f, 3.0f};
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({140, 170, 210});
            sl::set_light_3d({{-0.3f, -1.0f, -0.4f}, 0.6f});
            for (int i = 0; i < 3; ++i)
            {
                sl::Material material;
                material.texture = textures[i];
                sl::draw_mesh(plane, sl::model_matrix({(i - 1) * 2.2f, 0.0f, -27.0f}, {0, 0, 0}), material);
            }
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {30, 30, 30});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 30, 60);
        sl::gprintf_center(24, {232, 236, 244}, "Left: bilinear only   Middle: mipmaps   Right: mipmaps + 16x anisotropy");
        sl::gprintf_center(552, {232, 236, 244}, "Space pauses camera sway, Escape exits");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(plane);
    for (sl::Bitmap *texture : textures)
        sl::destroy_bitmap(texture);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
