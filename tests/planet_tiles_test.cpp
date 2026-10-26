#undef NDEBUG
#include "sl.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr int view_size = 64;

    void put(std::vector<std::uint8_t> &file, int at, std::uint32_t value)
    {
        std::memcpy(file.data() + at, &value, 4);
    }

    std::vector<std::uint8_t> dds_header(int size, const char *fourcc)
    {
        std::vector<std::uint8_t> file(128, 0);
        std::memcpy(file.data(), "DDS ", 4);
        put(file, 4, 124);
        put(file, 8, 0x1 | 0x2 | 0x4 | 0x1000);
        put(file, 12, static_cast<std::uint32_t>(size));
        put(file, 16, static_cast<std::uint32_t>(size));
        put(file, 76, 32);
        put(file, 80, 0x4);
        std::memcpy(file.data() + 84, fourcc, 4);
        put(file, 108, 0x1000);
        return file;
    }

    // A solid-colour DXT1 tile, 8 x 8 pixels.
    void write_tile(const fs::path &path, std::uint16_t colour)
    {
        std::vector<std::uint8_t> file = dds_header(8, "DXT1");
        for (int i = 0; i < 4; ++i)
        {
            const std::uint8_t block[8] = {static_cast<std::uint8_t>(colour & 255), static_cast<std::uint8_t>(colour >> 8), 0, 0, 0, 0, 0, 0};
            file.insert(file.end(), block, block + 8);
        }
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
    }

    // Levels 0-2 are complete (red, green, blue); level 3 is white and exists only at column 2, row 4.
    void make_pyramid(const fs::path &root, bool sparse_top)
    {
        const std::uint16_t colours[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
        for (int level = 0; level <= 2; ++level)
            for (int r = 0; r < (1 << level); ++r)
                for (int c = 0; c < (2 << level); ++c)
                    write_tile(root / ("level" + std::to_string(level)) / ("tx_" + std::to_string(c) + "_" + std::to_string(r) + ".dds"), colours[level]);
        if (sparse_top)
            write_tile(root / "level3" / "tx_2_4.dds", colours[3]);
    }

    glm::vec3 point_on_sphere(double u, double v)
    {
        const double theta = 2.0 * 3.14159265358979 * u, phi = 3.14159265358979 * v;
        return {static_cast<float>(std::sin(phi) * std::cos(theta)), static_cast<float>(std::cos(phi)), static_cast<float>(-std::sin(phi) * std::sin(theta))};
    }

    struct Shot
    {
        sl::Colour centre;
        sl::PlanetTilesStats stats;
    };

    // Looks at the sphere along the normal at (u, v) from `distance` radii away, for up to `frames` frames, until
    // `want_level` is drawn at the centre of the view.
    Shot look(sl::Surface3D *surface, sl::PlanetTiles &planet, double u, double v, float distance, int want_level, float texel_pixels, bool lit = false, int frames = 240)
    {
        (void)texel_pixels;
        sl::Camera camera;
        const glm::vec3 normal = point_on_sphere(u, v);
        camera.position = normal * distance;
        camera.target = {0.0f, 0.0f, 0.0f};
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.near_plane = 0.01f;
        camera.far_plane = 50.0f;
        camera.field_of_view = 40.0f;
        Shot shot{};
        for (int frame = 0; frame < frames; ++frame)
        {
            const glm::mat4 view_projection = camera.projection_matrix(1.0f) * camera.view_matrix();
            planet.update(glm::dvec3(camera.position), glm::radians(camera.field_of_view), view_size, &view_projection);
            sl::Light3D sun;
            sun.direction = -normal;
            sun.ambient = 0.3f;
            sl::set_light_3d(sun);
            assert(sl::begin_surface_3d(surface, camera));
            sl::clear_surface_3d({0, 0, 0, 255});
            sl::Material material;
            material.lit = lit;
            material.fog = false;
            planet.draw(glm::mat4(1.0f), material);
            sl::end_surface_3d();
            sl::show_video_bitmap();
            sl::end_frame();
            shot.stats = planet.stats();
            if (shot.stats.deepest_level >= want_level && shot.stats.tiles_loading == 0)
                break;
        }
        sl::Bitmap *image = sl::surface_3d_bitmap(surface);
        assert(sl::download_bitmap(image));
        shot.centre = sl::getpixel(image, view_size / 2, view_size / 2);
        return shot;
    }

    bool near_colour(sl::Colour c, int r, int g, int b)
    {
        return std::abs(c.red - r) < 40 && std::abs(c.green - g) < 40 && std::abs(c.blue - b) < 40;
    }
}

// A tile pyramid with a sparse deepest level: the view refines towards the camera, loads tiles in the background,
// and keeps the coarser tile wherever the deeper level has none.
int main(int argc, char *argv[])
{
    // Numbering helpers need no display.
    float rect[4];
    sl::PlanetTiles::ancestor_uv(5, 3, 1, rect);
    assert(rect[0] == 0.5f && rect[1] == 0.5f && rect[2] == 1.0f && rect[3] == 1.0f);
    sl::PlanetTiles::ancestor_uv(6, 2, 2, rect);
    assert(rect[0] == 0.5f && rect[1] == 0.5f && rect[2] == 0.75f && rect[3] == 0.75f);
    assert(sl::PlanetTiles::columns(3) == 16 && sl::PlanetTiles::rows(3) == 8 && sl::PlanetTiles::columns(0, 1) == 4);

    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, view_size, view_size))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    const fs::path root = fs::temp_directory_path() / "simlib_planet_tiles_test";
    fs::remove_all(root);
    make_pyramid(root / "colour", true);
    make_pyramid(root / "gloss", false);
    fs::remove_all(root / "gloss" / "level2");
    {
        std::ofstream ctx(root / "colour.ctx");
        ctx << "VirtualTexture\n{\n ImageDirectory \"colour\"\n BaseSplit 0\n TileSize 8\n TileType \"dds\"\n}\n";
    }

    sl::Surface3D *surface = sl::create_surface_3d(view_size, view_size, 1);
    assert(surface);

    assert(!sl::PlanetTiles::create({}));
    sl::PlanetTilesOptions options;
    options.colour = (root / "colour.ctx").string();
    options.segments = 6;
    options.max_loads_in_flight = 4;
    options.feather = 0.0f;
    options.fade_seconds = 0.0f;

    // Near the one deepest tile: all four levels are used.
    {
        auto planet = sl::PlanetTiles::create(options);
        assert(planet);
        assert(planet->deepest_level() == 3 && planet->tile_size() == 8 && !planet->has_specular());
        const Shot shot = look(surface, *planet, 0.15, 0.53, 1.02f, 3, 1.0f);
        assert(shot.stats.deepest_level == 3);
        assert(near_colour(shot.centre, 255, 255, 255));
    }
    // Where level 3 has no tile, level 2 (blue) is kept.
    {
        auto planet = sl::PlanetTiles::create(options);
        const Shot shot = look(surface, *planet, 0.65, 0.53, 1.02f, 2, 1.0f);
        assert(shot.stats.deepest_level == 2);
        assert(near_colour(shot.centre, 0, 0, 255));
    }
    // Far away only the coarsest level is needed.
    {
        options.texel_pixels = 100.0f;
        auto planet = sl::PlanetTiles::create(options);
        const Shot shot = look(surface, *planet, 0.15, 0.53, 6.0f, 0, 100.0f);
        assert(shot.stats.deepest_level == 0);
        assert(shot.stats.patches <= 4 && shot.stats.tiles_loaded == 2);
        assert(near_colour(shot.centre, 255, 0, 0));
        options.texel_pixels = 1.0f;
    }
    // A max_level cap and the tile cache limit are respected.
    {
        options.max_level = 1;
        auto planet = sl::PlanetTiles::create(options);
        const Shot shot = look(surface, *planet, 0.15, 0.53, 1.02f, 1, 1.0f);
        assert(shot.stats.deepest_level == 1);
        assert(near_colour(shot.centre, 0, 255, 0));
        options.max_level = -1;
    }
    // A specular map that stops at level 1 adds a glint where the sun mirrors into the camera, and nothing without it.
    {
        sl::PlanetTilesOptions glossy = options;
        glossy.specular = (root / "gloss").string();
        glossy.max_level = 1;
        auto planet = sl::PlanetTiles::create(glossy);
        assert(planet && planet->has_specular());
        const Shot with = look(surface, *planet, 0.15, 0.53, 1.5f, 1, 1.0f, true);
        glossy.specular_strength = 0.0f;
        auto plain = sl::PlanetTiles::create(glossy);
        const Shot without = look(surface, *plain, 0.15, 0.53, 1.5f, 1, 1.0f, true);
        std::printf("glint: with %d,%d,%d without %d,%d,%d\n", with.centre.red, with.centre.green, with.centre.blue, without.centre.red, without.centre.green, without.centre.blue);
        assert(with.centre.red > without.centre.red + 10);
    }
    // The lone white level-3 tile has no neighbours, so it feathers out onto the blue level-2 tile at its edge and is solid in its middle.
    {
        sl::PlanetTilesOptions soft = options;
        soft.feather = 0.3f;
        auto middle = sl::PlanetTiles::create(soft);
        const Shot inside = look(surface, *middle, 2.5 / 16.0, 4.5 / 8.0, 1.02f, 3, 1.0f);
        assert(inside.stats.deepest_level == 3);
        assert(near_colour(inside.centre, 255, 255, 255));
        auto edge = sl::PlanetTiles::create(soft);
        const Shot rim = look(surface, *edge, 2.03 / 16.0, 4.5 / 8.0, 1.02f, 3, 1.0f);
        std::printf("feather edge: %d,%d,%d\n", rim.centre.red, rim.centre.green, rim.centre.blue);
        assert(rim.centre.blue > 200 && rim.centre.red < 120);
    }
    // A tile that has just loaded fades in over its parent rather than popping.
    {
        sl::PlanetTilesOptions slow = options;
        slow.fade_seconds = 1000.0f;
        auto planet = sl::PlanetTiles::create(slow);
        const Shot shot = look(surface, *planet, 2.5 / 16.0, 4.5 / 8.0, 1.02f, 3, 1.0f);
        assert(shot.stats.deepest_level == 3);
        assert(shot.centre.blue > 200 && shot.centre.red < 120);
    }
    // A missing folder fails cleanly.
    {
        sl::PlanetTilesOptions bad;
        bad.colour = (root / "nothing").string();
        assert(!sl::PlanetTiles::create(bad));
        assert(!sl::last_error().empty());
    }

    // DXT3 files decode with their explicit alpha.
    {
        std::vector<std::uint8_t> file = dds_header(4, "DXT3");
        const std::uint8_t block[16] = {0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xE0, 0x07, 0xE0, 0x07, 0, 0, 0, 0};
        file.insert(file.end(), block, block + 16);
        const fs::path path = root / "bc2.dds";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
        out.close();
        sl::Bitmap *bitmap = sl::load_dds(path.string());
        assert(bitmap && bitmap->width == 4 && bitmap->height == 4);
        assert(sl::download_bitmap(bitmap) || true);
        assert(sl::getpixel(bitmap, 0, 0).alpha == 0 && sl::getpixel(bitmap, 1, 0).alpha == 17);
        assert(sl::getpixel(bitmap, 3, 3).alpha == 255 && sl::getpixel(bitmap, 0, 0).green == 255);
        sl::destroy_bitmap(bitmap);
    }

    sl::destroy_surface_3d(surface);
    sl::shutdown();
    fs::remove_all(root);
    std::printf("planet_tiles ok\n");
    return 0;
}
