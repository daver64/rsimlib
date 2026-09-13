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

    // 7. Test multitexturing shader
    sl::Shader multi_shader;
    const std::string vert_source =
        "#version 430 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aTexCoord;\n"
        "layout(location = 2) in vec4 aColor;\n"
        "uniform mat4 uProjection;\n"
        "out vec2 uv;\n"
        "void main() {\n"
        "    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);\n"
        "    uv = aTexCoord;\n"
        "}\n";
    const std::string frag_source =
        "#version 430 core\n"
        "uniform sampler2D texA;\n"
        "uniform sampler2D texB;\n"
        "in vec2 uv;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec4 a = texture(texA, uv);\n"
        "    vec4 b = texture(texB, uv);\n"
        "    fragColor = vec4(a.r + b.r, a.g + b.g, a.b + b.b, 1.0);\n"
        "}\n";

    if (multi_shader.load(vert_source, frag_source, {"texA", "texB"}, {}))
    {
        sl::Bitmap *bmpA = sl::create_video_bitmap(32, 32);
        sl::Bitmap *bmpB = sl::create_video_bitmap(32, 32);
        sl::clear_to_colour(bmpA, sl::Colour{100, 0, 0, 255});
        sl::clear_to_colour(bmpB, sl::Colour{50, 120, 0, 255});
        sl::upload_bitmap(bmpA);
        sl::upload_bitmap(bmpB);

        sl::Bitmap *multi_rt = sl::create_render_target(64, 64);
        assert(sl::begin_render_target(multi_rt));
        sl::clear_render_target(sl::Colour{0, 0, 0, 255});

        assert(multi_shader.set_texture("texB", 1, bmpB));
        assert(multi_shader.draw_textured_quad(bmpA, 0, 0, 32, 32));
        sl::end_render_target();

        assert(sl::download_bitmap(multi_rt));
        sl::Colour combined = sl::getpixel(multi_rt, 16, 16);
        assert(combined.red >= 148 && combined.red <= 152);
        assert(combined.green >= 118 && combined.green <= 122);
        assert(combined.blue == 0);

        sl::destroy_bitmap(bmpA);
        sl::destroy_bitmap(bmpB);
        sl::destroy_bitmap(multi_rt);
        multi_shader.reset();
    }

    // 8. Test compute shader and StorageBuffer upload/readback
    sl::StorageBuffer storage_buf(16 * sizeof(float));
    assert(storage_buf.is_valid());
    std::vector<float> input_data(16);
    for (std::size_t i = 0; i < 16; ++i) input_data[i] = static_cast<float>(i + 1);
    assert(storage_buf.upload(input_data));

    const std::string comp_source =
        "#version 430 core\n"
        "layout(local_size_x = 16) in;\n"
        "layout(std430, binding = 0) buffer DataBuffer {\n"
        "    float values[];\n"
        "};\n"
        "void main() {\n"
        "    uint id = gl_GlobalInvocationID.x;\n"
        "    if (id < values.length()) values[id] *= 2.0;\n"
        "}\n";

    sl::Shader comp_shader;
    if (comp_shader.load_compute(comp_source))
    {
        assert(comp_shader.is_valid());
        assert(storage_buf.bind(0));
        assert(sl::dispatch_compute_for(comp_shader, 16, 1, 1, 16, 1, 1));
        sl::compute_barrier();

        std::vector<float> output_data(16, 0.0f);
        assert(storage_buf.readback(output_data));
        for (std::size_t i = 0; i < 16; ++i)
        {
            assert(output_data[i] == static_cast<float>(i + 1) * 2.0f);
        }
        comp_shader.reset();
    }
    storage_buf.destroy();

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
