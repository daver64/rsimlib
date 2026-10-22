#undef NDEBUG
#include "sl.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

// Conformance scenes for Material::blend. Each draws flat-coloured slabs in front of the camera and checks the
// centre pixel against values that every backend must agree on (run with --gl and --vulkan).
namespace
{
    int failures = 0;

    void check(bool ok, const char *name, sl::Colour got)
    {
        if (!ok)
        {
            ++failures;
            std::printf("FAIL: %s (got %d, %d, %d)\n", name, got.red, got.green, got.blue);
        }
    }

    bool near(sl::Colour got, int r, int g, int b, int tolerance = 5)
    {
        return std::abs(got.red - r) <= tolerance && std::abs(got.green - g) <= tolerance &&
               std::abs(got.blue - b) <= tolerance;
    }

    sl::Material flat(sl::Colour tint, sl::MaterialBlend blend = sl::MaterialBlend::opaque)
    {
        sl::Material material;
        material.tint = tint;
        material.lit = false;
        material.fog = false;
        material.blend = blend;
        return material;
    }

    glm::mat4 slab(float z) { return glm::scale(glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, z}), {6.0f, 6.0f, 0.2f}); }

    struct Scene
    {
        sl::Surface3D *surface = nullptr;
        sl::Mesh *cube = nullptr;
        sl::Camera camera;

        template <typename Draw>
        sl::Colour render(Draw draw)
        {
            sl::begin_surface_3d(surface, camera);
            sl::clear_surface_3d({0, 0, 0, 255});
            draw();
            sl::end_surface_3d();
            // Vulkan records into the frame; submit it before reading the image back.
            sl::show_video_bitmap();
            sl::end_frame();
            sl::Bitmap *image = sl::surface_3d_bitmap(surface);
            if (!sl::download_bitmap(image))
                std::abort();
            return sl::getpixel(image, 64, 64);
        }
    };
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 128, 128))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    Scene scene;
    scene.surface = sl::create_surface_3d(128, 128, 1);
    scene.cube = sl::create_cube_mesh(1.0f);
    if (!scene.surface || !scene.cube)
        return 1;
    scene.camera.position = {0.0f, 0.0f, 4.0f};

    const sl::Material red = flat({255, 0, 0, 255});
    const sl::Material green = flat({0, 255, 0, 255});
    const sl::Material half_blue = flat({0, 0, 255, 128}, sl::MaterialBlend::alpha);
    const sl::Material half_blue_add = flat({0, 0, 255, 128}, sl::MaterialBlend::additive);
    const sl::Material half_green = flat({0, 255, 0, 128}, sl::MaterialBlend::alpha);

    // Alpha blending mixes the translucent colour with what is behind it.
    sl::Colour got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
        sl::draw_mesh(scene.cube, slab(0.0f), half_blue);
    });
    check(near(got, 127, 0, 128), "alpha over opaque", got);

    // Zero opacity changes nothing; full opacity replaces.
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
        sl::draw_mesh(scene.cube, slab(0.0f), flat({0, 0, 255, 0}, sl::MaterialBlend::alpha));
    });
    check(near(got, 255, 0, 0), "alpha 0 is invisible", got);
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
        sl::draw_mesh(scene.cube, slab(0.0f), flat({0, 0, 255, 255}, sl::MaterialBlend::alpha));
    });
    check(near(got, 0, 0, 255), "alpha 255 replaces", got);

    // Additive adds the scaled colour to the background.
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), flat({100, 0, 0, 255}));
        sl::draw_mesh(scene.cube, slab(0.0f), half_blue_add);
    });
    check(near(got, 100, 0, 128), "additive", got);

    // Without a blend mode, tint alpha is ignored and the draw stays opaque.
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
        sl::draw_mesh(scene.cube, slab(0.0f), flat({0, 0, 255, 128}));
    });
    check(near(got, 0, 0, 255), "opaque ignores tint alpha", got);

    // Blended draws do not write depth: a farther opaque slab drawn afterwards still shows.
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(0.0f), half_green);
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
    });
    check(near(got, 255, 0, 0), "blended draw writes no depth", got);

    // ...but they do test depth: a translucent slab behind an opaque one is hidden.
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(0.0f), green);
        sl::draw_mesh(scene.cube, slab(-1.0f), half_blue);
    });
    check(near(got, 0, 255, 0), "blended draw tests depth", got);

    // Two translucent layers compose in draw order (far first): blue over green over black.
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), half_green);
        sl::draw_mesh(scene.cube, slab(0.0f), half_blue);
    });
    check(near(got, 0, 64, 128), "two layers, far then near", got);
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(0.0f), half_blue);
        sl::draw_mesh(scene.cube, slab(-1.0f), half_green);
    });
    // Drawn near first, the far layer is depth-tested against nothing (no depth written): green over blue.
    check(near(got, 0, 128, 64), "two layers, near then far", got);

    // Texture alpha multiplies in: a fully transparent texel leaves the background alone.
    sl::Bitmap *clear_texture = sl::create_bitmap(4, 4);
    sl::clear_to_colour(clear_texture, {255, 255, 255, 0});
    sl::Material textured = flat({0, 0, 255, 255}, sl::MaterialBlend::alpha);
    textured.texture = clear_texture;
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
        sl::draw_mesh(scene.cube, slab(0.0f), textured);
    });
    check(near(got, 255, 0, 0), "texture alpha 0 is invisible", got);
    sl::Bitmap *half_texture = sl::create_bitmap(4, 4);
    sl::clear_to_colour(half_texture, {255, 255, 255, 128});
    textured.texture = half_texture;
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
        sl::draw_mesh(scene.cube, slab(0.0f), textured);
    });
    check(near(got, 127, 0, 128), "texture alpha scales opacity", got);

    // Instanced draws blend the same way, and the per-instance tint alpha counts too.
    sl::Mesh *unit = sl::create_cube_mesh(1.0f);
    std::vector<sl::InstanceData3D> instances(1);
    instances[0].model = slab(0.0f);
    instances[0].tint = {255, 255, 255, 128};
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
        sl::draw_mesh_instanced(unit, instances, flat({0, 0, 255, 255}, sl::MaterialBlend::alpha));
    });
    check(near(got, 127, 0, 128), "instanced alpha", got);

    // An alpha-blended draw leaves later opaque draws unaffected in state (opaque after blended is opaque).
    got = scene.render([&] {
        sl::draw_mesh(scene.cube, slab(-1.0f), red);
        sl::draw_mesh(scene.cube, slab(0.0f), half_blue);
        sl::draw_mesh(scene.cube, slab(0.5f), green);
    });
    check(near(got, 0, 255, 0), "opaque after blended stays opaque", got);

    // The frame that just ended issued three plain draws, one of them blended.
    const sl::FrameStats stats = sl::get_frame_stats();
    check(stats.mesh_draws == 3 && stats.blended_draws == 1 && stats.instanced_draws == 0, "frame stats count draws", got);

    // Precipitation3D frees itself on destruction, whether that happens before or after shutdown.
    {
        sl::Precipitation3D scoped;
        scoped.weather.set_immediately({sl::PrecipitationType::rain, 1.0f});
        scoped.update(0.1f, scene.camera);
        sl::begin_surface_3d(scene.surface, scene.camera);
        scoped.draw(scene.camera);
        sl::end_surface_3d();
    }
    auto outlives_display = std::make_unique<sl::Precipitation3D>();
    outlives_display->weather.set_immediately({sl::PrecipitationType::snow, 1.0f});
    outlives_display->update(0.1f, scene.camera);
    sl::begin_surface_3d(scene.surface, scene.camera);
    outlives_display->draw(scene.camera);
    sl::end_surface_3d();

    // Creating a surface builds its 3D pipelines up front, so the first draws compile nothing (Vulkan and
    // Direct3D 12 count pipeline builds; OpenGL always reports 0, which passes trivially).
    {
        sl::end_frame();
        sl::end_frame();
        const std::uint64_t before = sl::get_frame_stats().pipelines_created;
        sl::Surface3D *warm = sl::create_surface_3d(64, 64, 4);
        sl::end_frame();
        const std::uint64_t built = sl::get_frame_stats().pipelines_created;
        check(warm != nullptr, "warm-up surface", {0, 0, 0});
        check(built >= before, "pipeline counter never decreases", {0, 0, 0});
        sl::set_environment_3d({});
        sl::begin_surface_3d(warm, scene.camera);
        sl::clear_surface_3d({0, 0, 0, 255});
        sl::draw_sky_3d();
        sl::draw_mesh(scene.cube, slab(0.0f), red);
        sl::draw_mesh(scene.cube, slab(0.5f), half_blue);
        sl::draw_mesh(scene.cube, slab(1.0f), half_blue_add);
        sl::end_surface_3d();
        sl::show_video_bitmap();
        sl::end_frame();
        sl::end_frame();
        check(sl::get_frame_stats().pipelines_created == built, "first 3D draws build no pipelines", {0, 0, 0});
        sl::clear_environment_3d();
        sl::destroy_surface_3d(warm);
    }

    sl::destroy_bitmap(clear_texture);
    sl::destroy_bitmap(half_texture);
    sl::destroy_mesh(unit);
    sl::destroy_mesh(scene.cube);
    sl::destroy_surface_3d(scene.surface);
    sl::shutdown();
    outlives_display.reset();
    if (failures)
    {
        std::printf("%d blend checks failed\n", failures);
        return 1;
    }
    std::printf("blend3d_test passed\n");
    return 0;
}
