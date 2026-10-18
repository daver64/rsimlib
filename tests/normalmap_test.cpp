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
    // Render-target readback after a lighting pass is not reliable on Vulkan.
    if (sl::graphics_backend() == sl::GraphicsBackend::vulkan)
    {
        std::printf("Vulkan, skipping\n");
        sl::shutdown();
        return 0;
    }
    sl::Bitmap *paired = sl::create_bitmap(1, 1);
    sl::Bitmap *paired_normal = sl::create_bitmap(1, 1);
    sl::set_normal_map(paired, paired_normal);
    sl::destroy_bitmap(paired_normal);
    assert(paired->normal_map == nullptr);
    sl::destroy_bitmap(paired);

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

    // The dome is symmetric, so rotating or flipping the sprite must not change which side the light hits.
    auto draw_sprite = [&](int variant)
    {
        switch (variant)
        {
        case 1:
            sl::draw_sprite_rotated(colour, 128.0f, 128.0f, 180.0f);
            break;
        case 2:
            sl::draw_sprite_rotated(colour, 128.0f, 128.0f, 90.0f);
            break;
        case 3:
            sl::draw_sprite_h_flip(colour, 96.0f, 96.0f);
            break;
        default:
            sl::draw_sprite(colour, 96.0f, 96.0f);
        }
    };
    auto render_scene = [&](int variant)
    {
        sl::begin_render_target(scene);
        sl::clear_render_target({0, 0, 0, 255});
        draw_sprite(variant);
        sl::end_render_target();
        assert(sl::begin_normal_pass(normals));
        draw_sprite(variant);
        sl::end_normal_pass();
    };

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

    for (int variant = 0; variant < 4; ++variant)
    {
        render_scene(variant);
        const float flat_ratio = brightness(false, 108) / std::max(brightness(false, 148), 1.0f);
        const float normal_ratio = brightness(true, 108) / std::max(brightness(true, 148), 1.0f);
        std::printf("variant %d left/right ratio flat=%.2f normal=%.2f\n", variant, flat_ratio, normal_ratio);
        assert(normal_ratio > flat_ratio * 1.5f);
    }

    sl::wait_for_graphics();
    lighting.shutdown();
    sl::destroy_bitmap(normals);
    sl::destroy_bitmap(scene);
    sl::destroy_bitmap(normal);
    sl::destroy_bitmap(colour);
    sl::shutdown();
    return 0;
}
