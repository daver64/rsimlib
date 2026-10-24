#undef NDEBUG
#include "sl.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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

    sl::Colour draw_and_read(sl::Bitmap *texture, sl::Bitmap *target)
    {
        sl::begin_render_target(target);
        sl::clear_render_target({0, 0, 0, 255});
        sl::draw_sprite(texture, 0, 0);
        sl::end_render_target();
        sl::show_video_bitmap();
        sl::end_frame();
        assert(sl::download_bitmap(target));
        return sl::getpixel(target, 4, 4);
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

    if (gpu->compressed)
        assert(!sl::download_bitmap(gpu));
    assert(!sl::load_dds("does_not_exist.dds"));

    sl::destroy_bitmap(gpu);
    sl::destroy_bitmap(cpu);
    sl::destroy_bitmap(target);
    std::filesystem::remove(path);
    sl::shutdown();
    return 0;
}
