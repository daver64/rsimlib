#undef NDEBUG
#include "sl.h"

#include <cassert>
#include <cstdio>

namespace
{
    constexpr int size = 128;

    // Applies the effect to a frame split into a mid-grey and a bright-grey half and returns the (left, right) red values.
    void run(sl::Hdr &hdr, sl::Bitmap *scene, sl::Bitmap *output, int &left, int &right)
    {
        sl::begin_render_target(scene);
        sl::clear_render_target({0, 0, 0, 255});
        sl::rectfill(sl::screen, 0, 0, size / 2, size, {128, 128, 128, 255});
        sl::rectfill(sl::screen, size / 2, 0, size, size, {200, 200, 200, 255});
        sl::end_render_target();
        sl::begin_render_target(output);
        sl::clear_render_target({0, 0, 0, 255});
        hdr.apply(scene, 0, 0, size, size);
        sl::end_render_target();
        // Vulkan records into the frame; submit it before reading the image back.
        sl::show_video_bitmap();
        sl::end_frame();
        assert(sl::download_bitmap(output));
        left = sl::getpixel(output, size / 4, size / 2).red;
        right = sl::getpixel(output, 3 * size / 4, size / 2).red;
    }
}

// At exposure 1 the effect leaves the picture alone; raising exposure brightens it, but bright areas roll off
// below pure white instead of clipping the way a plain 2x multiply would.
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, size, size))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    sl::Bitmap *scene = sl::create_render_target(size, size);
    sl::Bitmap *output = sl::create_render_target(size, size);
    sl::Hdr hdr;
    assert(scene && output);
    assert(hdr.initialise());
    hdr.set_bloom_intensity(0.0f);

    int left = 0, right = 0;
    run(hdr, scene, output, left, right);
    std::printf("exposure 1: %d %d\n", left, right);
    assert(std::abs(left - 128) <= 4 && std::abs(right - 200) <= 4);

    hdr.set_exposure(2.0f);
    run(hdr, scene, output, left, right);
    std::printf("exposure 2: %d %d\n", left, right);
    assert(left > 150 && right > 200 && right < 250);

    hdr.set_exposure(0.5f);
    run(hdr, scene, output, left, right);
    std::printf("exposure 0.5: %d %d\n", left, right);
    assert(left < 110 && right < 180);

    sl::destroy_bitmap(scene);
    sl::destroy_bitmap(output);
    sl::shutdown();
    return 0;
}
