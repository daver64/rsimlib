#undef NDEBUG
#include "sl.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

namespace
{
    // An 8x8 BC1 image with two mip levels: level 0 is solid red, level 1 solid green.
    std::vector<std::uint8_t> make_dds()
    {
        std::vector<std::uint8_t> file(128, 0);
        auto put = [&](int at, std::uint32_t value) { std::memcpy(file.data() + at, &value, 4); };
        std::memcpy(file.data(), "DDS ", 4);
        put(4, 124);
        put(8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000);
        put(12, 8);
        put(16, 8);
        put(28, 2);
        put(76, 32);
        put(80, 0x4);
        std::memcpy(file.data() + 84, "DXT1", 4);
        put(108, 0x1000 | 0x8 | 0x400000);
        auto solid_block = [&](std::uint16_t colour)
        {
            const std::uint8_t block[8] = {static_cast<std::uint8_t>(colour & 255), static_cast<std::uint8_t>(colour >> 8), 0, 0, 0, 0, 0, 0};
            file.insert(file.end(), block, block + 8);
        };
        for (int i = 0; i < 4; ++i)
            solid_block(0xF800);
        solid_block(0x07E0);
        return file;
    }

    sl::Colour draw_and_read(sl::Bitmap *texture, sl::Bitmap *target, sl::Colour background = {0, 0, 0, 255})
    {
        sl::begin_render_target(target);
        sl::clear_render_target(background);
        sl::draw_sprite(texture, 0, 0);
        sl::end_render_target();
        sl::show_video_bitmap();
        sl::end_frame();
        assert(sl::download_bitmap(target));
        return sl::getpixel(target, 4, 4);
    }

    sl::Colour mesh_sample(sl::Bitmap *texture, float u, float v)
    {
        std::vector<sl::Vertex3D> vertices(4);
        const float xy[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (int i = 0; i < 4; ++i)
        {
            vertices[i].position[0] = xy[i][0];
            vertices[i].position[1] = xy[i][1];
            vertices[i].uv[0] = u;
            vertices[i].uv[1] = v;
        }
        sl::Mesh *mesh = sl::create_mesh(vertices, {0, 1, 2, 0, 2, 3});
        sl::Surface3D *surface = sl::create_surface_3d(32, 32, 1);
        assert(mesh && surface);
        sl::Camera camera;
        camera.position = {0, 0, 3};
        sl::Material material;
        material.texture = texture;
        material.lit = false;
        material.fog = false;
        material.cull = sl::CullMode::none;
        assert(sl::begin_surface_3d(surface, camera));
        sl::clear_surface_3d({0, 0, 0, 255});
        assert(sl::draw_mesh(mesh, glm::mat4(1.0f), material));
        sl::end_surface_3d();
        sl::show_video_bitmap();
        sl::end_frame();
        sl::Bitmap *image = sl::surface_3d_bitmap(surface);
        assert(sl::download_bitmap(image));
        const sl::Colour result = sl::getpixel(image, 16, 16);
        sl::destroy_surface_3d(surface);
        sl::destroy_mesh(mesh);
        return result;
    }
}

// A BC1 file loads straight to the GPU when the backend allows, otherwise through the CPU decoder; both draw red.
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 64, 64))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    const std::string path = (std::filesystem::temp_directory_path() / "simlib_dds_test.dds").string();
    {
        const std::vector<std::uint8_t> bytes = make_dds();
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    sl::Bitmap *target = sl::create_render_target(16, 16);
    assert(target);

    sl::Bitmap *gpu = sl::load_dds(path);
    assert(gpu && gpu->width == 8 && gpu->height == 8);
    std::printf("compressed upload: %s\n", gpu->compressed ? "yes" : "no (CPU decode)");
    sl::Colour pixel = draw_and_read(gpu, target);
    assert(pixel.red > 240 && pixel.green < 16 && pixel.blue < 16);

    sl::DdsOptions cpu_only;
    cpu_only.allow_compressed_upload = false;
    sl::Bitmap *cpu = sl::load_dds(path, cpu_only);
    assert(cpu && !cpu->compressed);
    const sl::Colour decoded = sl::getpixel(cpu, 3, 3);
    assert(decoded.red > 240 && decoded.green < 16 && decoded.alpha == 255);
    pixel = draw_and_read(cpu, target);
    assert(pixel.red > 240 && pixel.green < 16);

    const std::vector<std::uint8_t> bytes = make_dds();
    sl::Bitmap *memory_cpu = sl::load_dds_from_memory(bytes.data(), bytes.size(), cpu_only);
    assert(memory_cpu && !memory_cpu->compressed && memory_cpu->pixels == cpu->pixels);
    sl::Bitmap *memory_gpu = sl::load_dds_from_memory(bytes.data(), bytes.size());
    assert(memory_gpu && memory_gpu->width == 8 && memory_gpu->height == 8);
    pixel = draw_and_read(memory_gpu, target);
    assert(pixel.red > 240 && pixel.green < 16 && pixel.blue < 16);
    {
        std::vector<std::uint8_t> image = bytes;
        for (int block = 1; block < 4; ++block)
        {
            image[128 + block * 8] = 0xe0;
            image[129 + block * 8] = 0x07;
        }
        for (bool compressed : {false, true})
        {
            for (bool repeat : {false, true})
            {
                sl::DdsOptions opts;
                opts.allow_compressed_upload = compressed;
                opts.smooth = false;
                opts.repeat = repeat;
                sl::Bitmap *texture = sl::load_dds_from_memory(image.data(), image.size(), opts);
                assert(texture && texture->repeat == repeat);
                for (const auto &uv : {std::pair<float, float>{1.125f, 0.125f},
                                       std::pair<float, float>{0.125f, 1.125f},
                                       std::pair<float, float>{1.125f, 1.125f}})
                {
                    pixel = mesh_sample(texture, uv.first, uv.second);
                    assert(repeat ? pixel.red > 240 && pixel.green < 16 : pixel.green > 240 && pixel.red < 16);
                }
                if (repeat)
                {
                    pixel = mesh_sample(texture, -0.875f, -0.875f);
                    assert(pixel.red > 240 && pixel.green < 16);
                }
                sl::destroy_bitmap(texture);
            }
            sl::DdsOptions opts;
            opts.allow_compressed_upload = compressed;
            opts.smooth = true;
            opts.repeat = true;
            sl::Bitmap *texture = sl::load_dds_from_memory(image.data(), image.size(), opts);
            assert(texture);
            const sl::Colour before_seam = mesh_sample(texture, -0.01f, 0.125f);
            const sl::Colour after_seam = mesh_sample(texture, 0.99f, 0.125f);
            assert(std::abs(int(before_seam.red) - int(after_seam.red)) <= 2);
            assert(std::abs(int(before_seam.green) - int(after_seam.green)) <= 2);
            assert(before_seam.red > 20 && before_seam.green > 20);
            sl::destroy_bitmap(texture);
        }
        for (bool repeat : {false, true})
        {
            sl::Bitmap *texture = sl::create_bitmap(8, 8);
            assert(texture);
            sl::clear_to_colour(texture, {0, 255, 0, 255});
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                    sl::putpixel(texture, x, y, {255, 0, 0, 255});
            texture->repeat = repeat;
            texture->mipmaps = true;
            pixel = mesh_sample(texture, 1.125f, 1.125f);
            assert(repeat ? pixel.red > 240 && pixel.green < 16 : pixel.green > 240 && pixel.red < 16);
            sl::destroy_bitmap(texture);
        }
        sl::AsyncBitmapOptions async_options;
        async_options.repeat = true;
        sl::AsyncBitmap async = sl::load_bitmap_async([image]
        {
            sl::DdsOptions opts;
            opts.allow_compressed_upload = false;
            return sl::load_dds_from_memory(image.data(), image.size(), opts);
        }, async_options);
        assert(async.wait());
        sl::Bitmap *texture = async.get();
        assert(texture && texture->repeat);
        pixel = mesh_sample(texture, 1.125f, 1.125f);
        assert(pixel.red > 240 && pixel.green < 16);
        sl::destroy_bitmap(texture);
    }
    for (int kind = 0; kind < 4; ++kind)
    {
        std::vector<std::uint8_t> image(bytes.begin(), bytes.begin() + 128);
        const std::uint32_t flags = 0x1 | 0x2 | 0x4 | 0x1000;
        std::memcpy(image.data() + 8, &flags, 4);
        std::memcpy(image.data() + 84, kind == 0 ? "DXT3" : "DX10", 4);
        if (kind > 0)
        {
            image.resize(148, 0);
            const std::uint32_t format = kind == 1 ? 71 : kind == 2 ? 74 : 77;
            const std::uint32_t dimension = 3, array_size = 1;
            std::memcpy(image.data() + 128, &format, 4);
            std::memcpy(image.data() + 132, &dimension, 4);
            std::memcpy(image.data() + 140, &array_size, 4);
        }
        for (int block = 0; block < 4; ++block)
        {
            if (kind != 1)
                image.insert(image.end(), 8, 255);
            image.insert(image.end(), bytes.begin() + 128 + block * 8, bytes.begin() + 136 + block * 8);
        }
        sl::Bitmap *decoded_image = sl::load_dds_from_memory(image.data(), image.size(), cpu_only);
        sl::Bitmap *render_image = sl::load_dds_from_memory(image.data(), image.size());
        assert(decoded_image && render_image);
        const sl::Colour colour = sl::getpixel(decoded_image, 3, 3);
        assert(colour.red > 240 && colour.green < 16 && colour.blue < 16 && colour.alpha == 255);
        pixel = draw_and_read(render_image, target);
        assert(pixel.red > 240 && pixel.green < 16 && pixel.blue < 16);
        sl::destroy_bitmap(decoded_image);
        sl::destroy_bitmap(render_image);
    }
    for (bool dx10 : {false, true})
    {
        std::vector<std::uint8_t> image(bytes.begin(), bytes.begin() + 128);
        const std::uint32_t flags = 0x1 | 0x2 | 0x4 | 0x1000;
        std::memcpy(image.data() + 8, &flags, 4);
        if (dx10)
        {
            std::memcpy(image.data() + 84, "DX10", 4);
            image.resize(148, 0);
            const std::uint32_t format = 71, dimension = 3, array_size = 1;
            std::memcpy(image.data() + 128, &format, 4);
            std::memcpy(image.data() + 132, &dimension, 4);
            std::memcpy(image.data() + 140, &array_size, 4);
        }
        // c0 <= c1 selects the three-colour BC1 palette, whose fourth entry is transparent.
        const std::uint8_t block[8] = {0, 0, 0, 0xf8, 0xff, 0xff, 0xff, 0xff};
        for (int i = 0; i < 4; ++i)
            image.insert(image.end(), block, block + sizeof block);
        sl::Bitmap *decoded_image = sl::load_dds_from_memory(image.data(), image.size(), cpu_only);
        sl::Bitmap *render_image = sl::load_dds_from_memory(image.data(), image.size());
        assert(decoded_image && render_image);
        assert(sl::getpixel(decoded_image, 3, 3).alpha == 0);
        const sl::Colour background{0, 0, 255, 255};
        for (sl::Bitmap *texture : {decoded_image, render_image})
        {
            pixel = draw_and_read(texture, target, background);
            assert(pixel.red < 16 && pixel.green < 16 && pixel.blue > 240);
        }
        sl::destroy_bitmap(decoded_image);
        sl::destroy_bitmap(render_image);
        const std::size_t offset = dx10 ? 148 : 128;
        for (int i = 0; i < 4; ++i)
            for (int row = 0; row < 4; ++row)
                image[offset + static_cast<std::size_t>(i * 8 + 4 + row)] = 0xe4;
        decoded_image = sl::load_dds_from_memory(image.data(), image.size(), cpu_only);
        assert(decoded_image);
        const sl::Colour black = sl::getpixel(decoded_image, 0, 0);
        const sl::Colour red = sl::getpixel(decoded_image, 1, 0);
        const sl::Colour middle = sl::getpixel(decoded_image, 2, 0);
        const sl::Colour clear = sl::getpixel(decoded_image, 3, 0);
        assert(black.red == 0 && black.alpha == 255);
        assert(red.red == 255 && red.alpha == 255);
        assert(middle.red >= 127 && middle.red <= 128 && middle.alpha == 255);
        assert(clear.alpha == 0);
        sl::destroy_bitmap(decoded_image);
    }
    for (bool bc3 : {false, true})
    {
        std::vector<std::uint8_t> image(bytes.begin(), bytes.begin() + 128);
        const std::uint32_t flags = 0x1 | 0x2 | 0x4 | 0x1000;
        std::memcpy(image.data() + 8, &flags, 4);
        std::memcpy(image.data() + 84, bc3 ? "DXT5" : "DXT3", 4);
        const std::uint8_t colour[8] = {0, 0, 0, 0xf8, 0xff, 0xff, 0xff, 0xff};
        const std::uint8_t alpha2[8] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
        const std::uint8_t alpha3[8] = {128, 128, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < 4; ++i)
        {
            const std::uint8_t *alpha = bc3 ? alpha3 : alpha2;
            image.insert(image.end(), alpha, alpha + 8);
            image.insert(image.end(), colour, colour + 8);
        }
        sl::Bitmap *decoded_image = sl::load_dds_from_memory(image.data(), image.size(), cpu_only);
        assert(decoded_image);
        const sl::Colour colour_at = sl::getpixel(decoded_image, 3, 3);
        assert(colour_at.red == 170 && colour_at.alpha == (bc3 ? 128 : 136));
        sl::destroy_bitmap(decoded_image);
    }
    assert(!sl::load_dds_from_memory(nullptr, 0, cpu_only));
    assert(!sl::load_dds_from_memory(bytes.data(), 16, cpu_only));
    assert(!sl::load_dds_from_memory(bytes.data(), 130, cpu_only));
    sl::DdsOptions bounded = cpu_only;
    bounded.max_decoded_bytes = 8 * 8 * 4 - 1;
    assert(!sl::load_dds_from_memory(bytes.data(), bytes.size(), bounded));
    assert(sl::last_error().find("decoded DDS exceeds") != std::string::npos);
    bounded.max_decoded_bytes = 8 * 8 * 4;
    sl::Bitmap *bounded_image = sl::load_dds_from_memory(bytes.data(), bytes.size(), bounded);
    assert(bounded_image && bounded_image->width == 8 && bounded_image->height == 8);
    sl::destroy_bitmap(bounded_image);
    std::vector<std::uint8_t> oversized = bytes;
    const std::uint32_t dimension = 100000;
    std::memcpy(oversized.data() + 12, &dimension, sizeof(dimension));
    std::memcpy(oversized.data() + 16, &dimension, sizeof(dimension));
    assert(!sl::load_dds_from_memory(oversized.data(), oversized.size(), bounded));
    assert(sl::last_error().find("decoded DDS exceeds") != std::string::npos);

    if (gpu->compressed)
        assert(!sl::download_bitmap(gpu));
    assert(!sl::load_dds("does_not_exist.dds"));

    sl::destroy_bitmap(gpu);
    sl::destroy_bitmap(cpu);
    sl::destroy_bitmap(memory_cpu);
    sl::destroy_bitmap(memory_gpu);
    sl::destroy_bitmap(target);
    std::filesystem::remove(path);
    sl::shutdown();
    return 0;
}
