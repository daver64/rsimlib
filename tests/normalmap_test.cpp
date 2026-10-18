#undef NDEBUG
#include "sl.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

// Renders a normal-mapped dome lit from the left and checks the lit side is brighter than the far side.
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 256, 256))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    // Render-target readback after a lighting pass is only reliable on OpenGL.
    if (sl::graphics_backend() != sl::GraphicsBackend::opengl)
    {
        std::printf("not OpenGL, skipping\n");
        sl::shutdown();
        return 0;
    }
    constexpr int size = 64;
    sl::Bitmap *colour = sl::create_bitmap(size, size);
    sl::Bitmap *normal = sl::create_bitmap(size, size);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float dx = (x - 31.5f) / 28.0f;
            const float dy = (y - 31.5f) / 28.0f;
            const float d2 = std::min(dx * dx + dy * dy, 0.99f);
            sl::putpixel(colour, x, y, {200, 200, 200, 255});
            sl::putpixel(normal, x, y,
                         {static_cast<std::uint8_t>((dx * 0.5f + 0.5f) * 255.0f),
                          static_cast<std::uint8_t>((-dy * 0.5f + 0.5f) * 255.0f),
                          static_cast<std::uint8_t>((std::sqrt(1.0f - d2) * 0.5f + 0.5f) * 255.0f), 255});
        }
    }
    sl::set_normal_map(colour, normal);

    sl::Bitmap *scene = sl::create_render_target(256, 256);
    sl::Bitmap *normals = sl::create_render_target(256, 256);
    sl::LightingPass lighting;
    assert(scene && normals && lighting.initialise());
    lighting.set_ambient(0.0f);
    lighting.set_light_height(24.0f);

    sl::begin_render_target(scene);
    sl::clear_render_target({0, 0, 0, 255});
    sl::draw_sprite(colour, 96.0f, 96.0f);
    sl::end_render_target();
    assert(sl::begin_normal_pass(normals));
    sl::draw_sprite(colour, 96.0f, 96.0f);
    sl::end_normal_pass();

    sl::Light light;
    light.x = 40.0f;
    light.y = 128.0f;
    light.radius = 400.0f;
    light.intensity = 1.0f;

    auto brightness = [&](bool use_normals, int x)
    {
        // acquire_bitmap() only downloads once, so use a fresh target per measurement.
        sl::Bitmap *out = sl::create_render_target(256, 256);
        sl::begin_render_target(out);
        sl::clear_render_target({0, 0, 0, 255});
        if (use_normals)
            lighting.apply(scene, normals, {light}, 0, 0, 256, 256);
        else
            lighting.apply(scene, std::vector<sl::Light>{light}, 0, 0, 256, 256);
        sl::end_render_target();
        sl::acquire_bitmap(out);
        const float value = static_cast<float>(sl::getpixel(out, x, 128).red);
        sl::destroy_bitmap(out);
        return value;
    };

    const float flat_ratio = brightness(false, 108) / std::max(brightness(false, 148), 1.0f);
    const float normal_ratio = brightness(true, 108) / std::max(brightness(true, 148), 1.0f);
    std::printf("left/right ratio flat=%.2f normal=%.2f\n", flat_ratio, normal_ratio);
    assert(normal_ratio > flat_ratio * 1.5f);

    sl::wait_for_graphics();
    lighting.shutdown();
    sl::shutdown();
    return 0;
}
