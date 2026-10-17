#include "sl.h"
#include "surface3d.h"

#include <iostream>
#include <vector>

int main()
{
    if (!sl::set_graphics_backend(sl::GraphicsBackend::d3d12))
        return 1;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 320, 240))
    {
        std::cerr << "D3D12 initialization failed: " << sl::last_error() << '\n';
        return 1;
    }

    sl::clear_to_colour(18, 26, 38);
    sl::rectfill(sl::screen, 16, 16, 96, 96, sl::Colour{240, 80, 32, 255});

    sl::Bitmap *target = sl::create_render_target(32, 32);
    if (!target || !sl::begin_render_target(target))
    {
        std::cerr << "D3D12 render-target creation/bind failed: " << sl::last_error() << '\n';
        sl::destroy_bitmap(target);
        sl::display_shutdown();
        return 1;
    }
    sl::clear_render_target(sl::Colour{12, 24, 36, 255});
    sl::rectfill(target, 4, 4, 20, 20, sl::Colour{220, 120, 40, 255});
    sl::end_render_target();
    if (!sl::download_bitmap(target))
    {
        std::cerr << "D3D12 render-target readback failed: " << sl::last_error() << '\n';
        sl::destroy_bitmap(target);
        sl::display_shutdown();
        return 1;
    }
    const sl::Colour clear_pixel = sl::getpixel(target, 28, 28);
    const sl::Colour filled_pixel = sl::getpixel(target, 8, 8);
    if (clear_pixel.red != 12 || clear_pixel.green != 24 || clear_pixel.blue != 36 ||
        filled_pixel.red != 220 || filled_pixel.green != 120 || filled_pixel.blue != 40)
    {
        std::cerr << "D3D12 render-target readback returned unexpected pixels.\n";
        sl::destroy_bitmap(target);
        sl::display_shutdown();
        return 1;
    }
    sl::destroy_bitmap(target);

    sl::Surface3D *surface = sl::create_surface_3d(64, 64);
    sl::Mesh *cube = sl::create_cube_mesh(1.0f, sl::Colour{220, 60, 30, 255});
    if (!surface || !cube)
    {
        std::cerr << "D3D12 3D surface/mesh creation failed: " << sl::last_error() << '\n';
        sl::destroy_mesh(cube);
        sl::destroy_surface_3d(surface);
        sl::display_shutdown();
        return 1;
    }
    sl::Camera camera;
    camera.position = {2.5f, 2.2f, 4.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    if (!sl::begin_surface_3d(surface, camera))
    {
        std::cerr << "D3D12 3D surface bind failed: " << sl::last_error() << '\n';
        sl::destroy_mesh(cube);
        sl::destroy_surface_3d(surface);
        sl::display_shutdown();
        return 1;
    }
    sl::clear_surface_3d(sl::Colour{12, 24, 36, 255});
    sl::Material material;
    material.lit = false;
    if (!sl::draw_mesh(cube, glm::mat4(1.0f), material))
    {
        std::cerr << "D3D12 indexed mesh drawing failed: " << sl::last_error() << '\n';
        sl::end_surface_3d();
        sl::destroy_mesh(cube);
        sl::destroy_surface_3d(surface);
        sl::display_shutdown();
        return 1;
    }
    sl::end_surface_3d();
    sl::Bitmap *surface_bitmap = sl::surface_3d_bitmap(surface);
    if (!sl::download_bitmap(surface_bitmap))
    {
        std::cerr << "D3D12 3D surface readback failed: " << sl::last_error() << '\n';
        sl::destroy_mesh(cube);
        sl::destroy_surface_3d(surface);
        sl::display_shutdown();
        return 1;
    }
    bool found_mesh_pixel = false;
    for (int y = 0; y < surface_bitmap->height && !found_mesh_pixel; ++y)
    {
        for (int x = 0; x < surface_bitmap->width; ++x)
        {
            const sl::Colour pixel = sl::getpixel(surface_bitmap, x, y);
            if (pixel.red == 220 && pixel.green == 60 && pixel.blue == 30)
            {
                found_mesh_pixel = true;
                break;
            }
        }
    }
    if (!found_mesh_pixel)
    {
        std::cerr << "D3D12 3D mesh produced no colored pixels.\n";
        sl::destroy_mesh(cube);
        sl::destroy_surface_3d(surface);
        sl::display_shutdown();
        return 1;
    }
    sl::destroy_mesh(cube);
    sl::destroy_surface_3d(surface);

    sl::show_video_bitmap();
    sl::wait_for_graphics();
    sl::display_shutdown();
    return 0;
}
