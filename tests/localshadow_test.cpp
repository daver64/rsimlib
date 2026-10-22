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

    // Lights a floor with a cube hovering above it by one local light and returns the red channel of every pixel.
    std::vector<int> render(sl::Surface3D *surface, sl::Mesh *floor, sl::Mesh *cube, sl::LocalShadowMap3D *map,
                            bool spot, bool shadows)
    {
        sl::Light3D sun;
        sun.colour = {0.0f, 0.0f, 0.0f};
        sun.ambient = 0.0f;
        sl::set_light_3d(sun);
        sl::LocalLight3D lamp;
        lamp.position = {0.0f, 6.0f, 0.0f};
        lamp.intensity = 80.0f;
        lamp.range = 20.0f;
        lamp.cast_shadow = true;
        if (spot)
        {
            lamp.direction = {0.0f, -1.0f, 0.0f};
            lamp.inner_angle = 35.0f;
            lamp.outer_angle = 50.0f;
        }
        sl::set_local_lights_3d({lamp});
        sl::Material material;
        material.pbr = false;
        const glm::mat4 cube_model = glm::translate(glm::mat4(1.0f), glm::vec3(2.5f, 2.5f, 1.0f));
        auto draw_casters = [&]() { sl::draw_mesh(cube, cube_model, material); };

        sl::set_local_shadow_map_3d(nullptr);
        if (shadows)
        {
            assert(sl::render_local_shadows_3d(map, draw_casters));
            sl::set_local_shadow_map_3d(map);
        }
        assert(sl::begin_surface_3d(surface, make_camera()));
        sl::clear_surface_3d({0, 0, 0, 255});
        assert(sl::draw_mesh(floor, glm::mat4(1.0f), material));
        assert(sl::draw_mesh(cube, cube_model, material));
        sl::end_surface_3d();
        sl::set_local_shadow_map_3d(nullptr);
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

    void check(sl::Surface3D *surface, sl::Mesh *floor, sl::Mesh *cube, sl::LocalShadowMap3D *map, bool spot)
    {
        const std::vector<int> plain = render(surface, floor, cube, map, spot, false);
        const std::vector<int> shadowed = render(surface, floor, cube, map, spot, true);
        int darker = 0, brighter = 0;
        for (std::size_t i = 0; i < plain.size(); ++i)
        {
            if (shadowed[i] < plain[i] - 30)
                ++darker;
            else if (shadowed[i] > plain[i] + 10)
                ++brighter;
        }
        std::printf("%s light: %d pixels darker, %d brighter\n", spot ? "spot" : "point", darker, brighter);
        assert(darker > 40);
        assert(brighter < 20);
    }
}

// A point or spot light with cast_shadow set must leave the floor behind a caster dark and the rest of the image alone.
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, size, size))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    sl::Surface3D *surface = sl::create_surface_3d(size, size, 1);
    sl::LocalShadowMap3D *map = sl::create_local_shadow_map_3d(512, 2);
    sl::Mesh *floor = sl::create_plane_mesh(20.0f, 20.0f, {200, 200, 200, 255});
    sl::Mesh *cube = sl::create_cube_mesh(1.5f);
    assert(surface && map && floor && cube);

    check(surface, floor, cube, map, false);
    check(surface, floor, cube, map, true);

    sl::destroy_local_shadow_map_3d(map);
    sl::destroy_surface_3d(surface);
    sl::destroy_mesh(floor);
    sl::destroy_mesh(cube);
    sl::shutdown();
    std::printf("localshadow_test passed\n");
    return 0;
}
