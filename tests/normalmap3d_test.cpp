#undef NDEBUG
#include "sl.h"

#include <cassert>
#include <cstdio>

namespace
{
    sl::Bitmap *solid_normal(std::uint8_t r, std::uint8_t g, std::uint8_t b)
    {
        sl::Bitmap *bitmap = sl::create_bitmap(4, 4);
        sl::clear_to_colour(bitmap, {r, g, b, 255});
        return bitmap;
    }
}

// A cube face lit at 45 degrees: normal maps tilted toward and away from the light must brighten and darken it
// relative to the unperturbed (flat) result.
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 128, 128))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    if (sl::graphics_backend() != sl::GraphicsBackend::opengl)
    {
        std::printf("not OpenGL, skipping readback\n");
        sl::shutdown();
        return 0;
    }
    sl::Surface3D *surface = sl::create_surface_3d(128, 128);
    sl::Mesh *cube = sl::create_cube_mesh(1.0f);
    assert(surface && cube);
    sl::Bitmap *maps[3] = {nullptr, solid_normal(245, 128, 218), solid_normal(10, 128, 218)};

    sl::Camera camera;
    camera.position = {0.0f, 0.0f, 3.0f};
    sl::Light3D light;
    light.direction = {-1.0f, 0.0f, -1.0f};
    light.ambient = 0.0f;

    int brightness[3];
    for (int i = 0; i < 3; ++i)
    {
        assert(sl::begin_surface_3d(surface, camera));
        sl::clear_surface_3d({0, 0, 0, 255});
        sl::set_light_3d(light);
        sl::Material material;
        material.normal_map = maps[i];
        assert(sl::draw_mesh(cube, glm::mat4(1.0f), material));
        sl::end_surface_3d();
        sl::Bitmap *image = sl::surface_3d_bitmap(surface);
        assert(sl::download_bitmap(image));
        brightness[i] = sl::getpixel(image, 64, 64).red;
    }
    std::printf("flat %d, tilted %d / %d\n", brightness[0], brightness[1], brightness[2]);
    const int high = brightness[1] > brightness[2] ? brightness[1] : brightness[2];
    const int low = brightness[1] > brightness[2] ? brightness[2] : brightness[1];
    assert(high > brightness[0] + 20);
    assert(low < brightness[0] - 20);

    sl::shutdown();
    std::printf("normalmap3d_test passed\n");
    return 0;
}
