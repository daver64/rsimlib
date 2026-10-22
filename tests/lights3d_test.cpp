#undef NDEBUG
#include "sl.h"

#include <cassert>
#include <cstdio>
#include <vector>

namespace
{
    constexpr int size = 128;

    sl::Camera make_camera()
    {
        sl::Camera camera;
        camera.position = {0.0f, 9.0f, 6.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        camera.far_plane = 60.0f;
        return camera;
    }

    // Renders a floor with a cube hovering above it and returns the red channel of every pixel.
    std::vector<int> render(sl::Surface3D *surface, sl::Mesh *floor, sl::Mesh *cube, sl::ShadowMap3D *map, bool shadows)
    {
        sl::Light3D sun;
        sun.direction = {0.0f, -1.0f, -0.3f};
        sun.ambient = 0.2f;
        sl::set_light_3d(sun);
        sl::Material material;
        material.pbr = false;
        const glm::mat4 cube_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));

        sl::set_shadow_map_3d(nullptr);
        if (shadows)
        {
            assert(sl::begin_shadow_pass_3d(map, {0.0f, 0.0f, 0.0f}, 8.0f));
            sl::draw_mesh(floor, glm::mat4(1.0f), material);
            sl::draw_mesh(cube, cube_model, material);
            sl::end_shadow_pass_3d();
            sl::set_shadow_map_3d(map);
        }
        assert(sl::begin_surface_3d(surface, make_camera()));
        sl::clear_surface_3d({0, 0, 0, 255});
        assert(sl::draw_mesh(floor, glm::mat4(1.0f), material));
        assert(sl::draw_mesh(cube, cube_model, material));
        sl::end_surface_3d();
        sl::set_shadow_map_3d(nullptr);
        // Vulkan records into the frame; submit it before reading the image back.
        sl::show_video_bitmap();
        sl::end_frame();
        sl::Bitmap *image = sl::surface_3d_bitmap(surface);
        assert(sl::download_bitmap(image));
        std::vector<int> pixels;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                pixels.push_back(sl::getpixel(image, x, y).red);
        return pixels;
    }
}

// Local lights must brighten what they reach, and a sun shadow map must darken the floor behind a caster while leaving
// the rest of the image alone.
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, size, size))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    sl::Surface3D *surface = sl::create_surface_3d(size, size, 1);
    sl::ShadowMap3D *map = sl::create_shadow_map_3d(512);
    sl::Mesh *floor = sl::create_plane_mesh(20.0f, 20.0f, {200, 200, 200, 255});
    sl::Mesh *cube = sl::create_cube_mesh(1.0f);
    assert(surface && map && floor && cube);

    const std::vector<int> plain = render(surface, floor, cube, map, false);
    const std::vector<int> shadowed = render(surface, floor, cube, map, true);
    int darker = 0, brighter = 0;
    for (std::size_t i = 0; i < plain.size(); ++i)
    {
        if (shadowed[i] < plain[i] - 40)
            ++darker;
        else if (shadowed[i] > plain[i] + 10)
            ++brighter;
    }
    std::printf("shadow: %d pixels darker, %d brighter\n", darker, brighter);
    assert(darker > 30);
    assert(brighter < 20);

    sl::LocalLight3D lamp;
    lamp.position = {3.0f, 1.5f, 3.0f};
    lamp.colour = {1.0f, 1.0f, 1.0f};
    lamp.intensity = 20.0f;
    lamp.range = 12.0f;
    sl::set_local_lights_3d({lamp});
    assert(sl::get_local_lights_3d().size() == 1);
    const std::vector<int> lit = render(surface, floor, cube, map, false);
    int lamp_brighter = 0;
    for (std::size_t i = 0; i < plain.size(); ++i)
        if (lit[i] > plain[i] + 30)
            ++lamp_brighter;
    std::printf("local light: %d pixels brighter\n", lamp_brighter);
    assert(lamp_brighter > 200);
    sl::set_local_lights_3d({});
    assert(sl::get_local_lights_3d().empty());

    sl::destroy_shadow_map_3d(map);
    sl::destroy_surface_3d(surface);
    sl::destroy_mesh(floor);
    sl::destroy_mesh(cube);
    sl::shutdown();
    std::printf("lights3d_test passed\n");
    return 0;
}
