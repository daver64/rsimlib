#undef NDEBUG
#include "sl.h"

#include <cstdio>

// The icon API must accept a bitmap, reject bad input, and restore the default without a window failing.
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) || !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 128, 128))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    int failures = 0;
    auto expect = [&](bool ok, const char *name) {
        if (!ok)
        {
            ++failures;
            std::printf("FAIL: %s\n", name);
        }
    };
    expect(!sl::set_window_icon(static_cast<sl::Bitmap *>(nullptr)), "null bitmap is rejected");
    expect(!sl::set_window_icon(static_cast<const char *>(nullptr)), "null path is rejected");
    expect(!sl::set_window_icon("/nonexistent/icon.png"), "missing file is rejected");
    sl::Bitmap *icon = sl::create_bitmap(32, 32);
    expect(icon != nullptr, "create icon bitmap");
    if (icon)
    {
        sl::clear_to_colour(icon, {255, 0, 128});
        expect(sl::set_window_icon(icon), "bitmap icon is applied");
        sl::destroy_bitmap(icon);
    }
    expect(sl::set_window_icon(SIMLIB_TEST_ICON), "icon file is applied");
    sl::reset_window_icon();
    return failures;
}
