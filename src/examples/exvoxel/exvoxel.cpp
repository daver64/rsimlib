#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <utility>
#include <vector>

namespace
{
    // Block identifiers
    constexpr sl::BlockId BLOCK_GRASS = 1;
    constexpr sl::BlockId BLOCK_DIRT = 2;
    constexpr sl::BlockId BLOCK_STONE = 3;
    constexpr sl::BlockId BLOCK_COBBLE = 4;
    constexpr sl::BlockId BLOCK_WOOD = 5;
    constexpr sl::BlockId BLOCK_LEAVES = 6;
    constexpr sl::BlockId BLOCK_BRICK = 7;
    constexpr sl::BlockId BLOCK_BEDROCK = 8;
    constexpr sl::BlockId BLOCK_SAND = 9;
    constexpr sl::BlockId BLOCK_GLASS = 10;
    constexpr sl::BlockId BLOCK_TORCH = 11;
    constexpr sl::BlockId BLOCK_BASALT = 12;
    constexpr sl::BlockId BLOCK_REGOLITH = 13;
    constexpr sl::BlockId BLOCK_IRON_ORE = 14;
    constexpr sl::BlockId BLOCK_GOLD_ORE = 15;
    constexpr sl::BlockId BLOCK_CRYSTAL = 16;
    constexpr sl::BlockId BLOCK_ICE = 17;

    struct BlockTypeInfo
    {
        sl::BlockId id;
        const char *name;
        sl::Colour preview_colour;
    };

    const BlockTypeInfo PALETTE[] = {
        {BLOCK_REGOLITH, "Regolith", {150, 145, 138, 255}},
        {BLOCK_STONE, "Chondrite", {140, 140, 140, 255}},
        {BLOCK_BASALT, "Basalt", {60, 64, 80, 255}},
        {BLOCK_IRON_ORE, "Iron Ore", {190, 110, 70, 255}},
        {BLOCK_GOLD_ORE, "Gold Ore", {235, 200, 60, 255}},
        {BLOCK_CRYSTAL, "Crystal", {90, 220, 255, 255}},
        {BLOCK_ICE, "Ice", {190, 225, 245, 255}},
        {BLOCK_BRICK, "Hull Plate", {180, 80, 60, 255}},
        {BLOCK_GLASS, "Glass", {180, 220, 240, 160}},
        {BLOCK_TORCH, "Torch", {255, 170, 60, 255}},
    };
    constexpr int PALETTE_SIZE = static_cast<int>(sizeof(PALETTE) / sizeof(PALETTE[0]));

    enum class GameplayEventType
    {
        block_destroyed,
        block_placed,
        player_collision
    };

    struct GameplayEvent
    {
        GameplayEventType type;
        glm::ivec3 block_position;
        sl::BlockId block_id;
    };

    class GameplayEventDispatcher
    {
    public:
        using Callback = std::function<void(const GameplayEvent &)>;

        void add_listener(Callback callback)
        {
            callbacks_.push_back(std::move(callback));
        }

        void dispatch(const GameplayEvent &event) const
        {
            for (const Callback &callback : callbacks_)
                callback(event);
        }

    private:
        std::vector<Callback> callbacks_;
    };

    // Simple pseudo-random hash for procedural texture generation
    inline float noise2d(int x, int y, int seed)
    {
        std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u +
                          static_cast<std::uint32_t>(seed) * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return static_cast<float>(h & 0xffffu) / 32767.5f - 1.0f;
    }

    void set_tile_pixel(sl::Bitmap *atlas, int tx, int ty, int px, int py, sl::Colour colour)
    {
        const int x = tx * 16 + px;
        const int y = ty * 16 + py;
        if (x >= 0 && x < atlas->width && y >= 0 && y < atlas->height)
        {
            std::uint8_t *ptr = &atlas->pixels[(static_cast<std::size_t>(y) * atlas->width + x) * 4];
            ptr[0] = colour.red;
            ptr[1] = colour.green;
            ptr[2] = colour.blue;
            ptr[3] = colour.alpha;
        }
    }

    sl::Bitmap *create_procedural_atlas()
    {
        constexpr int atlas_size = 256;
        sl::Bitmap *bitmap = sl::create_bitmap(atlas_size, atlas_size);
        if (!bitmap)
            return nullptr;
        bitmap->pixels.assign(static_cast<std::size_t>(atlas_size) * atlas_size * 4, 0);

        auto clamp_u8 = [](float val) -> std::uint8_t
        {
            return static_cast<std::uint8_t>(std::clamp(val, 0.0f, 255.0f));
        };

        // Tile (0, 0): Grass top
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const float n = noise2d(x, y, 101) * 20.0f;
                set_tile_pixel(bitmap, 0, 0, x, y, {clamp_u8(90.0f + n), clamp_u8(170.0f + n * 1.5f), clamp_u8(50.0f + n * 0.5f), 255});
            }

        // Tile (1, 0): Grass side
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                if (y < 4 + static_cast<int>(noise2d(x, 0, 102) * 2.0f))
                {
                    const float n = noise2d(x, y, 103) * 18.0f;
                    set_tile_pixel(bitmap, 1, 0, x, y, {clamp_u8(90.0f + n), clamp_u8(170.0f + n * 1.5f), clamp_u8(50.0f + n * 0.5f), 255});
                }
                else
                {
                    const float n = noise2d(x, y, 104) * 20.0f;
                    set_tile_pixel(bitmap, 1, 0, x, y, {clamp_u8(125.0f + n), clamp_u8(85.0f + n * 0.7f), clamp_u8(50.0f + n * 0.4f), 255});
                }
            }

        // Tile (2, 0): Dirt
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const float n = noise2d(x, y, 201) * 22.0f;
                set_tile_pixel(bitmap, 2, 0, x, y, {clamp_u8(125.0f + n), clamp_u8(85.0f + n * 0.7f), clamp_u8(50.0f + n * 0.4f), 255});
            }

        // Tile (3, 0): Stone
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const float n = noise2d(x, y, 301) * 25.0f;
                set_tile_pixel(bitmap, 3, 0, x, y, {clamp_u8(135.0f + n), clamp_u8(135.0f + n), clamp_u8(135.0f + n), 255});
            }

        // Tile (4, 0): Cobblestone
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const bool edge = (x % 4 == 0) || (y % 4 == 0) || ((x + y) % 7 == 0);
                const float n = noise2d(x, y, 401) * 25.0f;
                if (edge)
                    set_tile_pixel(bitmap, 4, 0, x, y, {clamp_u8(60.0f + n * 0.5f), clamp_u8(60.0f + n * 0.5f), clamp_u8(60.0f + n * 0.5f), 255});
                else
                    set_tile_pixel(bitmap, 4, 0, x, y, {clamp_u8(130.0f + n), clamp_u8(130.0f + n), clamp_u8(130.0f + n), 255});
            }

        // Tile (5, 0): Wood Plank
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const bool seam = (y % 4 == 0) || ((x == 0 || x == 8) && (y % 8 < 4));
                const float n = noise2d(x, y, 501) * 15.0f;
                if (seam)
                    set_tile_pixel(bitmap, 5, 0, x, y, {100, 65, 35, 255});
                else
                    set_tile_pixel(bitmap, 5, 0, x, y, {clamp_u8(165.0f + n), clamp_u8(120.0f + n * 0.8f), clamp_u8(70.0f + n * 0.5f), 255});
            }

        // Tile (6, 0): Leaves
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const float n = noise2d(x, y, 601);
                if (n < -0.4f)
                    set_tile_pixel(bitmap, 6, 0, x, y, {30, 90, 20, 255});
                else if (n > 0.3f)
                    set_tile_pixel(bitmap, 6, 0, x, y, {80, 180, 40, 255});
                else
                    set_tile_pixel(bitmap, 6, 0, x, y, {50, 140, 30, 255});
            }

        // Tile (7, 0): Brick
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const int row = y / 4;
                const int col_offset = (row % 2 == 0) ? 0 : 4;
                const bool mortar = (y % 4 == 0) || ((x + col_offset) % 8 == 0);
                const float n = noise2d(x, y, 701) * 20.0f;
                if (mortar)
                    set_tile_pixel(bitmap, 7, 0, x, y, {200, 195, 185, 255});
                else
                    set_tile_pixel(bitmap, 7, 0, x, y, {clamp_u8(175.0f + n), clamp_u8(75.0f + n * 0.5f), clamp_u8(55.0f + n * 0.4f), 255});
            }

        // Tile (8, 0): Bedrock
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const float n = noise2d(x, y, 801) * 35.0f;
                set_tile_pixel(bitmap, 8, 0, x, y, {clamp_u8(50.0f + n), clamp_u8(50.0f + n), clamp_u8(50.0f + n), 255});
            }

        // Tile (9, 0): Sand
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const float n = noise2d(x, y, 901) * 15.0f;
                set_tile_pixel(bitmap, 9, 0, x, y, {clamp_u8(215.0f + n), clamp_u8(205.0f + n), clamp_u8(140.0f + n * 0.6f), 255});
            }

        // Tile (10, 0): Glass
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const bool border = (x == 0 || x == 15 || y == 0 || y == 15);
                const bool streak = (x == y && x >= 3 && x <= 6) || (x == y && x >= 10 && x <= 12);
                if (border)
                    set_tile_pixel(bitmap, 10, 0, x, y, {220, 240, 255, 255});
                else if (streak)
                    set_tile_pixel(bitmap, 10, 0, x, y, {240, 250, 255, 220});
                else
                    set_tile_pixel(bitmap, 10, 0, x, y, {180, 220, 250, 80});
            }

        // Tile (11, 0): Torch
        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                sl::Colour colour{65, 42, 28, 255};
                if (x >= 6 && x <= 9 && y >= 7 && y <= 13)
                    colour = {125, 76, 38, 255};
                if ((x >= 6 && x <= 9 && y >= 3 && y <= 6) ||
                    (x >= 5 && x <= 10 && y == 5))
                    colour = {255, 116, 24, 255};
                if ((x == 7 || x == 8) && y >= 3 && y <= 5)
                    colour = {255, 224, 72, 255};
                set_tile_pixel(bitmap, 11, 0, x, y, colour);
            }
        }

        // Asteroid rock tiles: base colour with noise, optionally speckled with ore blobs
        const auto rock_tile = [&](int tx, sl::Colour base, float amp, int seed,
                                   bool ore, sl::Colour ore_colour)
        {
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                {
                    const float n = noise2d(x, y, seed) * amp;
                    sl::Colour c{clamp_u8(base.red + n), clamp_u8(base.green + n), clamp_u8(base.blue + n), 255};
                    if (ore && noise2d(x / 2, y / 2, seed + 7) > 0.2f)
                    {
                        const float m = noise2d(x, y, seed + 13) * 18.0f;
                        c = {clamp_u8(ore_colour.red + m), clamp_u8(ore_colour.green + m), clamp_u8(ore_colour.blue + m), 255};
                    }
                    set_tile_pixel(bitmap, tx, 0, x, y, c);
                }
        };
        rock_tile(12, {58, 62, 78, 255}, 30.0f, 1201, false, {});
        rock_tile(13, {150, 145, 138, 255}, 28.0f, 1301, false, {});
        rock_tile(14, {120, 118, 116, 255}, 24.0f, 1401, true, {196, 108, 62, 255});
        rock_tile(15, {120, 118, 116, 255}, 24.0f, 1501, true, {240, 205, 60, 255});

        // Tile (16, 0): Crystal - bright cyan facets
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const bool facet = ((x + y) % 6 == 0) || ((x - y + 16) % 8 == 0);
                const float n = noise2d(x, y, 1601) * 25.0f;
                if (facet)
                    set_tile_pixel(bitmap, 16, 0, x, y, {200, 250, 255, 255});
                else
                    set_tile_pixel(bitmap, 16, 0, x, y, {clamp_u8(70.0f + n), clamp_u8(200.0f + n), clamp_u8(245.0f + n * 0.3f), 255});
            }

        rock_tile(17, {185, 220, 242, 255}, 22.0f, 1701, false, {});

        bitmap->ram_dirty = true;
        return bitmap;
    }

    void setup_block_registry()
    {
        // 1. Grass: top is tile (0,0), bottom is tile (2,0), sides are tile (1,0)
        sl::BlockDefinition grass;
        grass.tiles[static_cast<int>(sl::VoxelFace::top)] = {0, 0};
        grass.tiles[static_cast<int>(sl::VoxelFace::bottom)] = {2, 0};
        for (int f = 2; f < 6; ++f)
            grass.tiles[f] = {1, 0};
        sl::register_block_definition(BLOCK_GRASS, grass);

        // 2. Dirt: tile (2, 0)
        sl::BlockDefinition dirt;
        for (int f = 0; f < 6; ++f)
            dirt.tiles[f] = {2, 0};
        sl::register_block_definition(BLOCK_DIRT, dirt);

        // 3. Stone: tile (3, 0)
        sl::BlockDefinition stone;
        for (int f = 0; f < 6; ++f)
            stone.tiles[f] = {3, 0};
        sl::register_block_definition(BLOCK_STONE, stone);

        // 4. Cobble: tile (4, 0)
        sl::BlockDefinition cobble;
        for (int f = 0; f < 6; ++f)
            cobble.tiles[f] = {4, 0};
        sl::register_block_definition(BLOCK_COBBLE, cobble);

        // 5. Wood: tile (5, 0)
        sl::BlockDefinition wood;
        for (int f = 0; f < 6; ++f)
            wood.tiles[f] = {5, 0};
        sl::register_block_definition(BLOCK_WOOD, wood);

        // 6. Leaves: tile (6, 0), transparent
        sl::BlockDefinition leaves;
        leaves.is_transparent = true;
        for (int f = 0; f < 6; ++f)
            leaves.tiles[f] = {6, 0};
        sl::register_block_definition(BLOCK_LEAVES, leaves);

        // 7. Brick: tile (7, 0)
        sl::BlockDefinition brick;
        for (int f = 0; f < 6; ++f)
            brick.tiles[f] = {7, 0};
        sl::register_block_definition(BLOCK_BRICK, brick);

        // 8. Bedrock: tile (8, 0)
        sl::BlockDefinition bedrock;
        for (int f = 0; f < 6; ++f)
            bedrock.tiles[f] = {8, 0};
        sl::register_block_definition(BLOCK_BEDROCK, bedrock);

        // 9. Sand: tile (9, 0)
        sl::BlockDefinition sand;
        for (int f = 0; f < 6; ++f)
            sand.tiles[f] = {9, 0};
        sl::register_block_definition(BLOCK_SAND, sand);

        // 10. Glass: tile (10, 0), transparent
        sl::BlockDefinition glass;
        glass.is_transparent = true;
        for (int f = 0; f < 6; ++f)
            glass.tiles[f] = {10, 0};
        sl::register_block_definition(BLOCK_GLASS, glass);

        // 11. Torch: warm RGB emitter.
        sl::BlockDefinition torch;
        torch.is_solid = true; // solid so raycasts can target it for breaking/placing against
        torch.is_transparent = true;
        torch.light_emission = 15;
        torch.light_colour = {255, 150, 55, 255};
        for (int f = 0; f < 6; ++f)
            torch.tiles[f] = {11, 0};
        sl::register_block_definition(BLOCK_TORCH, torch);

        const auto simple_block = [](int tile)
        {
            sl::BlockDefinition def;
            for (int f = 0; f < 6; ++f)
                def.tiles[f] = {tile, 0};
            return def;
        };
        sl::register_block_definition(BLOCK_BASALT, simple_block(12));
        sl::register_block_definition(BLOCK_REGOLITH, simple_block(13));
        sl::register_block_definition(BLOCK_IRON_ORE, simple_block(14));
        sl::register_block_definition(BLOCK_GOLD_ORE, simple_block(15));

        // Crystal is decorative only; torches are the sole block light source
        sl::BlockDefinition crystal = simple_block(16);
        sl::register_block_definition(BLOCK_CRYSTAL, crystal);

        sl::register_block_definition(BLOCK_ICE, simple_block(17));
    }

    // Asteroid extents (block coordinates, centred on the origin)
    constexpr float ASTEROID_RADIUS_X = 28.0f;
    constexpr float ASTEROID_RADIUS_Y = 20.0f;
    constexpr float ASTEROID_RADIUS_Z = 24.0f;

    void carve_sphere(sl::VoxelWorld &world, float cx, float cy, float cz, float radius)
    {
        const int r = static_cast<int>(std::ceil(radius));
        for (int z = -r; z <= r; ++z)
            for (int y = -r; y <= r; ++y)
                for (int x = -r; x <= r; ++x)
                {
                    if (static_cast<float>(x * x + y * y + z * z) > radius * radius)
                        continue;
                    const int bx = static_cast<int>(std::floor(cx)) + x;
                    const int by = static_cast<int>(std::floor(cy)) + y;
                    const int bz = static_cast<int>(std::floor(cz)) + z;
                    if (world.get_block(bx, by, bz) != sl::BLOCK_AIR)
                        world.set_block(bx, by, bz, sl::BLOCK_AIR);
                }
    }

    void generate_asteroid(sl::VoxelWorld &world)
    {
        sl::Generator shape(42);
        shape.set_frequency(0.07f);
        sl::Generator rock(7);
        rock.set_frequency(0.06f);
        sl::Generator iron(11);
        iron.set_frequency(0.22f);
        sl::Generator gold(23);
        gold.set_frequency(0.3f);
        sl::Generator crystal(31);
        crystal.set_frequency(0.28f);
        sl::Generator ice(47);
        ice.set_frequency(0.18f);
        sl::Generator tunnel_a(59);
        tunnel_a.set_frequency(0.08f);
        sl::Generator tunnel_b(61);
        tunnel_b.set_frequency(0.08f);

        constexpr int ex = 36, ey = 28, ez = 32;
        for (int z = -ez; z <= ez; ++z)
            for (int y = -ey; y <= ey; ++y)
                for (int x = -ex; x <= ex; ++x)
                {
                    const float fx = static_cast<float>(x), fy = static_cast<float>(y), fz = static_cast<float>(z);
                    const float dx = fx / ASTEROID_RADIUS_X, dy = fy / ASTEROID_RADIUS_Y, dz = fz / ASTEROID_RADIUS_Z;
                    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                    const float limit = 1.0f + 0.22f * shape.get(fx, fy, fz);
                    if (d >= limit)
                        continue;

                    const float depth = limit - d;
                    sl::BlockId block = sl::BLOCK_AIR;
                    if (depth < 0.06f)
                        block = BLOCK_REGOLITH;
                    else if (rock.get(fx, fy, fz) > 0.2f)
                        block = BLOCK_BASALT;
                    else
                        block = BLOCK_STONE;

                    if (depth > 0.04f)
                    {
                        if (iron.get(fx, fy, fz) > 0.55f)
                            block = BLOCK_IRON_ORE;
                        else if (depth > 0.15f && gold.get(fx, fy, fz) > 0.62f)
                            block = BLOCK_GOLD_ORE;
                        else if (depth > 0.1f && ice.get(fx, fy, fz) > 0.6f)
                            block = BLOCK_ICE;
                        else if (depth > 0.1f && crystal.get(fx, fy, fz) > 0.64f)
                            block = BLOCK_CRYSTAL;
                    }

                    // Interconnected spaghetti tunnels where two noise fields are both near zero
                    if (depth > 0.04f && std::fabs(tunnel_a.get(fx, fy, fz)) < 0.07f &&
                        std::fabs(tunnel_b.get(fx, fy, fz)) < 0.07f)
                        block = sl::BLOCK_AIR;

                    if (block != sl::BLOCK_AIR)
                        world.set_block(x, y, z, block);
                }

        std::mt19937 rng(2024);
        const auto frand = [&rng](float lo, float hi)
        {
            return std::uniform_real_distribution<float>(lo, hi)(rng);
        };

        // Winding shafts that start on the surface and burrow toward the interior
        for (int i = 0; i < 18; ++i)
        {
            glm::vec3 dir = glm::normalize(glm::vec3(frand(-1, 1), frand(-1, 1), frand(-1, 1)));
            glm::vec3 pos = dir * glm::vec3(ASTEROID_RADIUS_X, ASTEROID_RADIUS_Y, ASTEROID_RADIUS_Z) * 1.1f;
            const float radius = frand(1.6f, 2.8f);
            const int steps = 30 + static_cast<int>(frand(0, 25));
            glm::vec3 heading = -dir;
            for (int s = 0; s < steps; ++s)
            {
                carve_sphere(world, pos.x, pos.y, pos.z, radius);
                heading = glm::normalize(heading + glm::vec3(frand(-0.35f, 0.35f), frand(-0.35f, 0.35f), frand(-0.35f, 0.35f)));
                pos += heading * 1.2f;
            }
            carve_sphere(world, pos.x, pos.y, pos.z, radius + 2.0f);
        }

        // Large internal caverns
        std::vector<glm::vec3> caverns;
        for (int i = 0; i < 5; ++i)
        {
            const glm::vec3 c(frand(-10, 10), frand(-6, 6), frand(-10, 10));
            carve_sphere(world, c.x, c.y, c.z, frand(3.5f, 5.5f));
            caverns.push_back(c);
        }

        // Light the caves: torches on cavern/tunnel floors
        int torches = 0;
        for (int attempt = 0; attempt < 4000 && torches < 28; ++attempt)
        {
            const int x = static_cast<int>(frand(-24, 24));
            const int y = static_cast<int>(frand(-14, 14));
            const int z = static_cast<int>(frand(-20, 20));
            if (world.get_block(x, y, z) == sl::BLOCK_AIR &&
                world.get_block(x, y - 1, z) != sl::BLOCK_AIR &&
                world.get_block(x, y - 1, z) != BLOCK_TORCH &&
                (std::fabs(static_cast<float>(x)) < 20.0f && std::fabs(static_cast<float>(z)) < 18.0f) &&
                std::sqrt(x * x / (ASTEROID_RADIUS_X * ASTEROID_RADIUS_X) + y * y / (ASTEROID_RADIUS_Y * ASTEROID_RADIUS_Y) +
                          z * z / (ASTEROID_RADIUS_Z * ASTEROID_RADIUS_Z)) < 0.7f)
            {
                world.set_block(x, y, z, BLOCK_TORCH);
                ++torches;
            }
        }
    }

    // True if any solid block overlaps the box [min, max)
    bool box_collides(const sl::VoxelWorld &world, const glm::vec3 &min, const glm::vec3 &max,
                      glm::ivec3 *colliding_block = nullptr)
    {
        for (int z = static_cast<int>(std::floor(min.z)); z <= static_cast<int>(std::floor(max.z - 1e-4f)); ++z)
            for (int y = static_cast<int>(std::floor(min.y)); y <= static_cast<int>(std::floor(max.y - 1e-4f)); ++y)
                for (int x = static_cast<int>(std::floor(min.x)); x <= static_cast<int>(std::floor(max.x - 1e-4f)); ++x)
                {
                    const sl::BlockId id = world.get_block(x, y, z);
                    if (id != sl::BLOCK_AIR && sl::get_block_definition(id).is_solid)
                    {
                        if (colliding_block)
                            *colliding_block = {x, y, z};
                        return true;
                    }
                }
        return false;
    }

    constexpr float PLAYER_HALF_WIDTH = 0.3f;
    constexpr float PLAYER_HEIGHT = 1.8f;
    constexpr float PLAYER_EYE = 1.6f;

    bool player_collides(const sl::VoxelWorld &world, const glm::vec3 &eye,
                         glm::ivec3 *colliding_block = nullptr)
    {
        const glm::vec3 feet(eye.x, eye.y - PLAYER_EYE, eye.z);
        return box_collides(world,
                            {feet.x - PLAYER_HALF_WIDTH, feet.y, feet.z - PLAYER_HALF_WIDTH},
                            {feet.x + PLAYER_HALF_WIDTH, feet.y + PLAYER_HEIGHT, feet.z + PLAYER_HALF_WIDTH},
                            colliding_block);
    }

    // Move the player along each axis in small steps, stopping on contact so it can slide along surfaces
    void move_with_collision(const sl::VoxelWorld &world, glm::vec3 &eye, const glm::vec3 &delta,
                             const GameplayEventDispatcher &events)
    {
        const int steps = std::max(1, static_cast<int>(std::ceil(glm::length(delta) / 0.2f)));
        const glm::vec3 step = delta / static_cast<float>(steps);
        std::vector<glm::ivec3> reported_collisions;
        for (int i = 0; i < steps; ++i)
            for (int axis = 0; axis < 3; ++axis)
            {
                if (step[axis] == 0.0f)
                    continue;
                glm::vec3 next = eye;
                next[axis] += step[axis];
                glm::ivec3 colliding_block;
                if (!player_collides(world, next, &colliding_block))
                    eye = next;
                else if (std::find(reported_collisions.begin(), reported_collisions.end(), colliding_block) == reported_collisions.end())
                {
                    reported_collisions.push_back(colliding_block);
                    events.dispatch({GameplayEventType::player_collision, colliding_block,
                                     world.get_block(colliding_block.x, colliding_block.y, colliding_block.z)});
                }
            }
    }

    // Procedural starfield, projected through the camera basis (alpha-less points drawn behind the scene)
    struct Star
    {
        glm::vec3 dir;
        std::uint8_t brightness;
    };

    std::vector<Star> make_stars(int count)
    {
        std::mt19937 rng(99);
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);
        std::vector<Star> stars;
        for (int i = 0; i < count; ++i)
        {
            glm::vec3 d(u(rng), u(rng), u(rng));
            if (glm::dot(d, d) < 0.01f)
                continue;
            stars.push_back({glm::normalize(d), static_cast<std::uint8_t>(110 + (rng() % 146))});
        }
        return stars;
    }
} // namespace

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv))
        return -1;

    constexpr int screen_w = 960;
    constexpr int screen_h = 600;

    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
    {
        std::fprintf(stderr, "Failed to initialize display: %s\n", sl::last_error().c_str());
        return -1;
    }

    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 1);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    setup_block_registry();

    sl::Bitmap *atlas_bitmap = create_procedural_atlas();
    sl::VoxelAtlas atlas;
    atlas.texture = atlas_bitmap;
    atlas.tiles_per_row = 16;
    atlas.tiles_per_col = 16;

    sl::VoxelWorld world;
    generate_asteroid(world);
    world.update_dirty_chunks(&atlas);

    GameplayEventDispatcher gameplay_events;
    gameplay_events.add_listener([](const GameplayEvent &event)
    {
        const char *name = "unknown";
        switch (event.type)
        {
        case GameplayEventType::block_destroyed:
            name = "block_destroyed";
            break;
        case GameplayEventType::block_placed:
            name = "block_placed";
            break;
        case GameplayEventType::player_collision:
            name = "player_collision";
            break;
        }
        std::printf("[event] %s block=%u at (%d, %d, %d)\n", name,
                    static_cast<unsigned int>(event.block_id),
                    event.block_position.x, event.block_position.y, event.block_position.z);
    });

    sl::Material voxel_material;
    voxel_material.texture = atlas_bitmap;
    voxel_material.lit = false; // illumination is baked per-vertex (sky + block light), so caves stay dark
    voxel_material.cull = sl::CullMode::back;

    // Camera state
    // Spawn hovering above the asteroid's highest block at the origin column
    int spawn_y = 40;
    while (spawn_y > -40 && world.get_block(0, spawn_y, 0) == sl::BLOCK_AIR)
        --spawn_y;
    glm::vec3 cam_pos(0.0f, static_cast<float>(spawn_y) + 16.0f, 22.0f);
    float yaw = -glm::half_pi<float>();
    float pitch = -0.7f;
    const std::vector<Star> stars = make_stars(500);

    int selected_palette_index = 0;
    bool mouse_captured = true;
    sl::set_relative_mouse_mode(true);
    bool running = true;
    int frame_count = 0;
    int prev_mouse_x = screen_w / 2;
    int prev_mouse_y = screen_h / 2;
    std::uint32_t last_time = sl::time_ms();

    while (running)
    {
        const std::uint32_t current_time = sl::time_ms();
        const float dt = std::clamp(static_cast<float>(current_time - last_time) * 0.001f, 0.001f, 0.1f);
        last_time = current_time;

        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            else if (event.type() == sl::Event::Type::mouse_button_down)
            {
                // Clicking inside the window recaptures mouse if released
                if (!mouse_captured)
                {
                    mouse_captured = true;
                    sl::set_relative_mouse_mode(true);
                }
            }
            else if (event.type() == sl::Event::Type::key_down)
            {
                if (event.key() == sl::Event::Key::escape)
                {
                    if (mouse_captured)
                    {
                        mouse_captured = false;
                        sl::set_relative_mouse_mode(false);
                    }
                    else
                    {
                        running = false;
                    }
                }
                else if (event.key() == sl::Event::Key::tab)
                {
                    mouse_captured = !mouse_captured;
                    sl::set_relative_mouse_mode(mouse_captured);
                }
                // Palette selection via numbers 1-9
                if (event.key() == sl::Event::Key::digit_1) selected_palette_index = 0;
                else if (event.key() == sl::Event::Key::digit_2) selected_palette_index = 1;
                else if (event.key() == sl::Event::Key::digit_3) selected_palette_index = 2;
                else if (event.key() == sl::Event::Key::digit_4) selected_palette_index = 3;
                else if (event.key() == sl::Event::Key::digit_5) selected_palette_index = 4;
                else if (event.key() == sl::Event::Key::digit_6) selected_palette_index = 5;
                else if (event.key() == sl::Event::Key::digit_7) selected_palette_index = 6;
                else if (event.key() == sl::Event::Key::digit_8) selected_palette_index = 7;
                else if (event.key() == sl::Event::Key::digit_9 && PALETTE_SIZE >= 9) selected_palette_index = 8;
                else if (event.key() == sl::Event::Key::digit_0 && PALETTE_SIZE >= 10) selected_palette_index = 9;
            }
            sl::display_handle_event(event);
        }

        // Mouse look
        if (mouse_captured)
        {
            int dx = 0, dy = 0;
            sl::mouse_delta(dx, dy);
            constexpr float sensitivity = 0.003f;
            yaw += static_cast<float>(dx) * sensitivity;
            pitch -= static_cast<float>(dy) * sensitivity;
        }
        else if (sl::mouse_button_down(sl::MouseButton::right))
        {
            // Right mouse drag also looks around if not captured
            const int mouse_x = sl::mouse_x();
            const int mouse_y = sl::mouse_y();
            const int dx = mouse_x - prev_mouse_x;
            const int dy = mouse_y - prev_mouse_y;
            constexpr float sensitivity = 0.005f;
            yaw += static_cast<float>(dx) * sensitivity;
            pitch -= static_cast<float>(dy) * sensitivity;
        }
        prev_mouse_x = sl::mouse_x();
        prev_mouse_y = sl::mouse_y();

        // Clamp pitch to avoid flip
        pitch = std::clamp(pitch, -1.55f, 1.55f);

        // Direction vectors
        const glm::vec3 forward(
            std::cos(pitch) * std::cos(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::sin(yaw)
        );
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up(0.0f, 1.0f, 0.0f);

        // Keyboard movement
        float move_speed = 12.0f;
        if (sl::key_down(sl::Key::left_shift) || sl::key_down(sl::Key::right_shift))
            move_speed = 24.0f;

        glm::vec3 move_dir(0.0f);
        if (sl::key_down(sl::Key::letter_w))
            move_dir += forward;
        if (sl::key_down(sl::Key::letter_s))
            move_dir -= forward;
        if (sl::key_down(sl::Key::letter_a))
            move_dir -= right;
        if (sl::key_down(sl::Key::letter_d))
            move_dir += right;
        if (sl::key_down(sl::Key::space))
            move_dir.y += 1.0f;
        if (sl::key_down(sl::Key::letter_c) || sl::key_down(sl::Key::left_control))
            move_dir.y -= 1.0f;

        if (glm::length(move_dir) > 0.01f)
            move_with_collision(world, cam_pos, glm::normalize(move_dir) * move_speed * dt, gameplay_events);

        // Block raycasting
        const sl::VoxelRaycastResult ray_hit = world.raycast(cam_pos, forward, 7.5f);

        // Handle block breaking and placement
        static bool prev_left_down = false;
        static bool prev_right_down = false;
        const bool left_down = sl::mouse_button_down(sl::MouseButton::left);
        const bool right_down = sl::mouse_button_down(sl::MouseButton::right);

        // Left click in captured mode: break block
        if (mouse_captured && left_down && !prev_left_down && ray_hit.hit)
        {
            const sl::BlockId destroyed_block = world.get_block(ray_hit.hit_block.x, ray_hit.hit_block.y, ray_hit.hit_block.z);
            if (destroyed_block != sl::BLOCK_AIR &&
                world.set_block(ray_hit.hit_block.x, ray_hit.hit_block.y, ray_hit.hit_block.z, sl::BLOCK_AIR))
            {
                world.update_dirty_chunks(&atlas);
                gameplay_events.dispatch({GameplayEventType::block_destroyed, ray_hit.hit_block, destroyed_block});
            }
        }

        // Right click in captured mode: place block
        if (mouse_captured && right_down && !prev_right_down && ray_hit.hit)
        {
            const sl::BlockId new_block = PALETTE[selected_palette_index].id;
            const glm::vec3 pmin(static_cast<float>(ray_hit.place_block.x), static_cast<float>(ray_hit.place_block.y),
                                 static_cast<float>(ray_hit.place_block.z));
            const glm::vec3 feet(cam_pos.x, cam_pos.y - PLAYER_EYE, cam_pos.z);
            const bool overlaps_player =
                pmin.x < feet.x + PLAYER_HALF_WIDTH && pmin.x + 1.0f > feet.x - PLAYER_HALF_WIDTH &&
                pmin.y < feet.y + PLAYER_HEIGHT && pmin.y + 1.0f > feet.y &&
                pmin.z < feet.z + PLAYER_HALF_WIDTH && pmin.z + 1.0f > feet.z - PLAYER_HALF_WIDTH;
            if (!overlaps_player || !sl::get_block_definition(new_block).is_solid)
            {
                if (world.set_block(ray_hit.place_block.x, ray_hit.place_block.y, ray_hit.place_block.z, new_block))
                {
                    world.update_dirty_chunks(&atlas);
                    gameplay_events.dispatch({GameplayEventType::block_placed, ray_hit.place_block, new_block});
                }
            }
        }

        prev_left_down = left_down;
        prev_right_down = right_down;

        // Setup camera
        sl::Camera camera;
        camera.position = cam_pos;
        camera.target = cam_pos + forward;
        camera.up = up;
        camera.field_of_view = 65.0f;
        camera.near_plane = 0.1f;
        camera.far_plane = 500.0f;

        // Render 3D scene
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({0, 0, 0, 0}); // Transparent so the starfield shows through

            world.draw(voxel_material);

            sl::end_surface_3d();
        }

        // Draw 3D surface to window screen
        sl::clear_to_colour(sl::screen, {2, 3, 8});
        for (const Star &star : stars)
        {
            const float depth = glm::dot(star.dir, forward);
            if (depth <= 0.05f)
                continue;
            const float focal = (static_cast<float>(screen_h) * 0.5f) / std::tan(glm::radians(65.0f) * 0.5f);
            const int sx = static_cast<int>(screen_w * 0.5f + glm::dot(star.dir, right) / depth * focal);
            const int sy = static_cast<int>(screen_h * 0.5f - glm::dot(star.dir, glm::cross(right, forward)) / depth * focal);
            if (sx >= 0 && sx < screen_w && sy >= 0 && sy < screen_h)
                sl::putpixel(sl::screen, sx, sy, {star.brightness, star.brightness, star.brightness, 255});
        }
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);

        // 2D HUD: Crosshair
        const float cx = static_cast<float>(screen_w) * 0.5f;
        const float cy = static_cast<float>(screen_h) * 0.5f;
        sl::Colour crosshair_col = ray_hit.hit ? sl::Colour{255, 255, 100, 240} : sl::Colour{255, 255, 255, 180};
        sl::line(sl::screen, cx - 8.0f, cy, cx + 8.0f, cy, crosshair_col);
        sl::line(sl::screen, cx, cy - 8.0f, cx, cy + 8.0f, crosshair_col);

        // 2D HUD: Palette bar at bottom
        constexpr float bar_w = 510.0f;
        constexpr float bar_h = 44.0f;
        const float bar_x = (screen_w - bar_w) * 0.5f;
        const float bar_y = screen_h - bar_h - 12.0f;

        sl::rectfill(sl::screen, bar_x, bar_y, bar_x + bar_w, bar_y + bar_h, {20, 24, 30, 210});
        sl::rect(sl::screen, bar_x, bar_y, bar_x + bar_w, bar_y + bar_h, {80, 100, 120, 255}, 1.5f);

        for (int i = 0; i < PALETTE_SIZE; ++i)
        {
            const float slot_x = bar_x + 8.0f + static_cast<float>(i) * 50.0f;
            const float slot_y = bar_y + 6.0f;
            const bool selected = (i == selected_palette_index);

            sl::rectfill(sl::screen, slot_x, slot_y, slot_x + 32.0f, slot_y + 32.0f, PALETTE[i].preview_colour);
            sl::rect(sl::screen, slot_x, slot_y, slot_x + 32.0f, slot_y + 32.0f,
                     selected ? sl::Colour{255, 230, 80, 255} : sl::Colour{60, 70, 80, 255},
                     selected ? 2.5f : 1.0f);

            char num_str[4];
            std::snprintf(num_str, sizeof(num_str), "%d", i + 1);
            sl::gprintf(static_cast<int>(slot_x + 2.0f), static_cast<int>(slot_y + 2.0f),
                        selected ? sl::Colour{255, 255, 0, 255} : sl::Colour{220, 220, 220, 255},
                        "%s", num_str);
        }

        // Stats & info
        sl::rectfill(sl::screen, 12.0f, 12.0f, 12.0f + 340.0f, 12.0f + 96.0f, {15, 20, 28, 200});
        sl::rect(sl::screen, 12.0f, 12.0f, 12.0f + 340.0f, 12.0f + 96.0f, {60, 90, 130, 255}, 1.0f);

        sl::gprintf(20, 18, {255, 255, 255}, "Simlib Asteroid Miner (%s)",sl::graphics_backend_name().c_str());
        sl::gprintf(20, 36, {160, 210, 255}, "Chunks: %zu | Triangles: %d",
                    world.chunk_count(), world.total_triangles());
        sl::gprintf(20, 54, {220, 220, 160}, "Pos: (%.1f, %.1f, %.1f)", cam_pos.x, cam_pos.y, cam_pos.z);
        if (ray_hit.hit)
        {
            sl::gprintf(20, 72, {255, 200, 120}, "Target: (%d, %d, %d) [Dist: %.1f]",
                        ray_hit.hit_block.x, ray_hit.hit_block.y, ray_hit.hit_block.z, ray_hit.distance);
        }
        else
        {
            sl::gprintf(20, 72, {140, 150, 160}, "Target: None");
        }

        // Controls help
        sl::gprintf_center(static_cast<int>(bar_y - 18.0f), {200, 210, 220},
                           mouse_captured
                               ? "[WASD] Fly  [Space/C] Up/Down  [L-Click] Break  [R-Click] Place  [Tab/Esc] Free cursor  [1-9,0] Block"
                               : "[Click Window/Tab] Lock mouse & look  [WASD] Move  [Esc] Exit  [1-9] Block");

        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();

        ++frame_count;
        const char *screenshot_env = std::getenv("RSIMLIB_SCREENSHOT");
        if (screenshot_env && frame_count == 10)
        {
            sl::save_bitmap(sl::screen, screenshot_env);
        }
        const char *frames_env = std::getenv("RSIMLIB_FRAMES");
        if (frames_env && frame_count >= std::atoi(frames_env))
        {
            running = false;
        }
    }

    sl::wait_for_graphics();
    sl::destroy_bitmap(atlas_bitmap);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
