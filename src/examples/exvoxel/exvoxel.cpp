#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

    struct BlockTypeInfo
    {
        sl::BlockId id;
        const char *name;
        sl::Colour preview_colour;
    };

    const BlockTypeInfo PALETTE[] = {
        {BLOCK_GRASS, "Grass", {100, 180, 60, 255}},
        {BLOCK_DIRT, "Dirt", {130, 90, 50, 255}},
        {BLOCK_STONE, "Stone", {140, 140, 140, 255}},
        {BLOCK_COBBLE, "Cobblestone", {110, 110, 110, 255}},
        {BLOCK_WOOD, "Wood Plank", {170, 120, 70, 255}},
        {BLOCK_LEAVES, "Leaves", {60, 150, 40, 220}},
        {BLOCK_BRICK, "Brick", {180, 80, 60, 255}},
        {BLOCK_SAND, "Sand", {220, 210, 140, 255}},
        {BLOCK_GLASS, "Glass", {180, 220, 240, 160}},
    };
    constexpr int PALETTE_SIZE = static_cast<int>(sizeof(PALETTE) / sizeof(PALETTE[0]));

    // Simple pseudo-random hash for procedural texture generation
    inline float noise2d(int x, int y, int seed)
    {
        int n = x + y * 57 + seed * 131;
        n = (n << 13) ^ n;
        return 1.0f - static_cast<float>((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
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
    }

    void generate_terrain(sl::VoxelWorld &world)
    {
        sl::Generator noise(42);
        noise.set_frequency(0.04f);

        constexpr int world_min_x = -24;
        constexpr int world_max_x = 24;
        constexpr int world_min_z = -24;
        constexpr int world_max_z = 24;

        for (int z = world_min_z; z <= world_max_z; ++z)
        {
            for (int x = world_min_x; x <= world_max_x; ++x)
            {
                const float n = (noise.get(static_cast<float>(x), static_cast<float>(z)) + 1.0f) * 0.5f;
                const int height = 8 + static_cast<int>(n * 10.0f);

                // Bedrock at y=0
                world.set_block(x, 0, z, BLOCK_BEDROCK);

                // Stone layer
                for (int y = 1; y < height - 3; ++y)
                    world.set_block(x, y, z, BLOCK_STONE);

                // Dirt layer
                for (int y = std::max(1, height - 3); y < height; ++y)
                    world.set_block(x, y, z, BLOCK_DIRT);

                // Top surface: grass or sand near water level
                if (height <= 9)
                    world.set_block(x, height, z, BLOCK_SAND);
                else
                    world.set_block(x, height, z, BLOCK_GRASS);

                // Scatter occasional trees
                if (x % 11 == 0 && z % 11 == 0 && height > 9 && height < 20)
                {
                    const int trunk_base = height + 1;
                    for (int ty = 0; ty < 4; ++ty)
                        world.set_block(x, trunk_base + ty, z, BLOCK_WOOD);

                    for (int lx = -1; lx <= 1; ++lx)
                    {
                        for (int lz = -1; lz <= 1; ++lz)
                        {
                            for (int ly = 2; ly <= 4; ++ly)
                            {
                                if (lx == 0 && lz == 0 && ly < 4)
                                    continue;
                                world.set_block(x + lx, trunk_base + ly, z + lz, BLOCK_LEAVES);
                            }
                        }
                    }
                }
            }
        }
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
    generate_terrain(world);
    world.update_dirty_chunks(&atlas);

    sl::Material voxel_material;
    voxel_material.texture = atlas_bitmap;
    voxel_material.lit = true;
    voxel_material.cull = sl::CullMode::back;

    sl::Light3D light;
    light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.7f));
    light.ambient = 0.4f;

    // Camera state
    glm::vec3 cam_pos(0.0f, 18.0f, 15.0f);
    float yaw = -glm::half_pi<float>();
    float pitch = -0.35f;

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
            move_dir += glm::vec3(forward.x, 0.0f, forward.z);
        if (sl::key_down(sl::Key::letter_s))
            move_dir -= glm::vec3(forward.x, 0.0f, forward.z);
        if (sl::key_down(sl::Key::letter_a))
            move_dir -= right;
        if (sl::key_down(sl::Key::letter_d))
            move_dir += right;
        if (sl::key_down(sl::Key::space))
            move_dir.y += 1.0f;
        if (sl::key_down(sl::Key::letter_c) || sl::key_down(sl::Key::left_control))
            move_dir.y -= 1.0f;

        if (glm::length(move_dir) > 0.01f)
            cam_pos += glm::normalize(move_dir) * move_speed * dt;

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
            world.set_block(ray_hit.hit_block.x, ray_hit.hit_block.y, ray_hit.hit_block.z, sl::BLOCK_AIR);
            world.update_dirty_chunks(&atlas);
        }

        // Right click in captured mode: place block
        if (mouse_captured && right_down && !prev_right_down && ray_hit.hit)
        {
            const sl::BlockId new_block = PALETTE[selected_palette_index].id;
            world.set_block(ray_hit.place_block.x, ray_hit.place_block.y, ray_hit.place_block.z, new_block);
            world.update_dirty_chunks(&atlas);
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
            sl::clear_surface_3d({135, 190, 235, 255}); // Sky blue
            sl::set_light_3d(light);

            world.draw(voxel_material);

            sl::end_surface_3d();
        }

        // Draw 3D surface to window screen
        sl::clear_to_colour(sl::screen, {20, 20, 20});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);

        // 2D HUD: Crosshair
        const float cx = static_cast<float>(screen_w) * 0.5f;
        const float cy = static_cast<float>(screen_h) * 0.5f;
        sl::Colour crosshair_col = ray_hit.hit ? sl::Colour{255, 255, 100, 240} : sl::Colour{255, 255, 255, 180};
        sl::line(sl::screen, cx - 8.0f, cy, cx + 8.0f, cy, crosshair_col);
        sl::line(sl::screen, cx, cy - 8.0f, cx, cy + 8.0f, crosshair_col);

        // 2D HUD: Palette bar at bottom
        constexpr float bar_w = 460.0f;
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

        sl::gprintf(20, 18, {255, 255, 255}, "Simlib Voxel Demo (%s)",
                    sl::graphics_backend() == sl::GraphicsBackend::vulkan ? "Vulkan" : "OpenGL");
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
                               ? "[WASD] Move  [Space/C] Up/Down  [L-Click] Break  [R-Click] Place  [Tab/Esc] Free cursor  [1-9] Block"
                               : "[Click Window/Tab] Lock mouse & look  [WASD] Move  [Esc] Exit  [1-9] Block");

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
