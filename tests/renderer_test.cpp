#include "sl.h"

#include <cassert>
#include <iostream>
#include <vector>

void test_backend(sl::GraphicsBackend backend)
{
    std::cout << "Testing graphics backend: "
              << (backend == sl::GraphicsBackend::opengl ? "OpenGL" : "Vulkan") << std::endl;

    sl::set_graphics_backend(backend);
    if (!sl::graphics_backend_available(backend))
    {
        std::cout << "  Backend not available, skipping." << std::endl;
        return;
    }

    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 320, 240))
    {
        std::cerr << "  Failed to initialize graphics mode: " << sl::last_error() << std::endl;
        return;
    }

    // 1. Test render target creation and clearing
    sl::Bitmap *rt = sl::create_render_target(128, 128);
    assert(rt != nullptr);
    assert(rt->width == 128);
    assert(rt->height == 128);

    assert(sl::begin_render_target(rt));
    sl::clear_render_target(sl::Colour{50, 100, 150, 255});

    // 2. Test filled rectangle rendering
    sl::rectfill(rt, 10, 10, 50, 50, sl::Colour{255, 0, 0, 255});

    // 3. Test text rendering with font atlas
    sl::Font *font = sl::get_default_monospace_font();
    if (font)
    {
        sl::textout(font, 10, 60, sl::Colour{255, 255, 255, 255}, "Test");
    }

    sl::end_render_target();

    // 4. Test download / pixel readback from render target
    assert(sl::download_bitmap(rt));
    assert(!rt->pixels.empty());

    // Check clear color in an un-drawn region (e.g. at 100, 100)
    sl::Colour bg_pixel = sl::getpixel(rt, 100, 100);
    assert(bg_pixel.red == 50);
    assert(bg_pixel.green == 100);
    assert(bg_pixel.blue == 150);

    // Check filled rectangle color (e.g. at 30, 30)
    sl::Colour rect_pixel = sl::getpixel(rt, 30, 30);
    assert(rect_pixel.red == 255);
    assert(rect_pixel.green == 0);
    assert(rect_pixel.blue == 0);

    // 5. Test flood fill on RAM bitmap
    sl::Bitmap *ram_bmp = sl::create_bitmap(32, 32);
    assert(ram_bmp != nullptr);
    sl::clear_to_colour(ram_bmp, sl::Colour{0, 0, 0, 255});
    sl::rect(ram_bmp, 5, 5, 25, 25, sl::Colour{255, 255, 255, 255}, 1.0f);
    sl::flood_fill(ram_bmp, 10, 10, sl::Colour{0, 255, 0, 255});
    sl::Colour filled_inner = sl::getpixel(ram_bmp, 10, 10);
    assert(filled_inner.red == 0 && filled_inner.green == 255 && filled_inner.blue == 0);
    sl::Colour outside_unfilled = sl::getpixel(ram_bmp, 2, 2);
    assert(outside_unfilled.red == 0 && outside_unfilled.green == 0 && outside_unfilled.blue == 0);
    sl::destroy_bitmap(ram_bmp);

    // 6. Test texture creation, upload, and download
    sl::Bitmap *tex_bmp = sl::create_video_bitmap(32, 32);
    assert(tex_bmp != nullptr);
    sl::clear_to_colour(tex_bmp, sl::Colour{200, 150, 50, 255});
    assert(sl::upload_bitmap(tex_bmp));

    sl::Bitmap *downloaded = sl::create_bitmap(32, 32);
    downloaded->gpu_texture = tex_bmp->gpu_texture;
    assert(sl::download_bitmap(downloaded));
    sl::Colour tex_pixel = sl::getpixel(downloaded, 16, 16);
    assert(tex_pixel.red == 200);
    assert(tex_pixel.green == 150);
    assert(tex_pixel.blue == 50);

    sl::destroy_bitmap(downloaded);
    sl::destroy_bitmap(tex_bmp);
    sl::destroy_bitmap(rt);

    sl::display_shutdown();
    std::cout << "  Backend tests passed successfully!" << std::endl;
}

int main(int argc, char **argv)
{
    test_backend(sl::GraphicsBackend::opengl);
    test_backend(sl::GraphicsBackend::vulkan);

    std::cout << "All renderer integration tests passed!" << std::endl;
    return 0;
}
