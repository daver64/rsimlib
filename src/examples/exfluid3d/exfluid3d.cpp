#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

// 3D cellular-automata fluids living inside a voxel world.
// A FluidSimulation3D covers a box of the world: solids are read from the VoxelWorld, and fluid cells are
// written back as translucent fluid blocks that the normal voxel mesher draws.
//   Mouse: aim, hold left button to spray the chosen element.   1-9/0: element   Space: pause   R: reset
//   A/D: orbit   W/S: tilt   Q/E: zoom

namespace
{
    constexpr int GRID_W = 48;
    constexpr int GRID_H = 36;
    constexpr int GRID_D = 48;
    constexpr int ORIGIN_X = -GRID_W / 2;
    constexpr int ORIGIN_Z = -GRID_D / 2;
    constexpr sl::BlockId BLOCK_STONE = 1;
    constexpr sl::BlockId BLOCK_WALL = 2;

    struct Brush
    {
        sl::FluidElement element;
        const char *name;
        float temp;
        std::uint8_t life;
    };

    const Brush BRUSHES[] = {
        {sl::FluidElement::water, "Water", 20.0f, 0},
        {sl::FluidElement::lava, "Lava", 1200.0f, 0},
        {sl::FluidElement::oil, "Oil", 20.0f, 0},
        {sl::FluidElement::acid, "Acid", 20.0f, 0},
        {sl::FluidElement::sand, "Sand", 20.0f, 0},
        {sl::FluidElement::gunpowder, "Gunpowder", 20.0f, 0},
        {sl::FluidElement::fire, "Fire", 800.0f, 80},
        {sl::FluidElement::steam, "Steam", 100.0f, 120},
        {sl::FluidElement::plant, "Plant", 20.0f, 0},
        {sl::FluidElement::wood, "Wood", 20.0f, 0},
    };
    constexpr int BRUSH_COUNT = static_cast<int>(sizeof(BRUSHES) / sizeof(BRUSHES[0]));

    void register_terrain_blocks()
    {
        sl::BlockDefinition stone;
        stone.tint = {128, 130, 138, 255};
        sl::register_block_definition(BLOCK_STONE, stone);
        sl::BlockDefinition wall;
        wall.tint = {88, 92, 104, 255};
        sl::register_block_definition(BLOCK_WALL, wall);
    }

    void build_scene(sl::VoxelWorld &world, const sl::FluidVoxelBlocks &fluid_blocks)
    {
        for (int z = 0; z < GRID_D; ++z)
            for (int x = 0; x < GRID_W; ++x)
            {
                const int wx = ORIGIN_X + x, wz = ORIGIN_Z + z;
                world.set_block(wx, 0, wz, BLOCK_STONE);
                const bool rim = x == 0 || z == 0 || x == GRID_W - 1 || z == GRID_D - 1;
                if (rim)
                    for (int y = 1; y <= 3; ++y)
                        world.set_block(wx, y, wz, BLOCK_WALL);
            }

        // A raised terrace along one side with a stepped ramp, so poured liquid runs downhill.
        for (int z = 1; z < GRID_D - 1; ++z)
            for (int x = 1; x < 16; ++x)
            {
                const int height = x < 11 ? 8 : 8 - (x - 10) * 2 + 1;
                for (int y = 1; y <= height; ++y)
                    world.set_block(ORIGIN_X + x, y, ORIGIN_Z + z, BLOCK_STONE);
            }

        // A flammable wooden hut with a plant bed beside it.
        for (int x = 28; x < 34; ++x)
            for (int z = 28; z < 34; ++z)
                for (int y = 1; y <= 4; ++y)
                {
                    const bool shell = x == 28 || x == 33 || z == 28 || z == 33 || y == 4;
                    if (shell)
                        world.set_block(ORIGIN_X + x, y, ORIGIN_Z + z, fluid_blocks.wood);
                }
        for (int x = 36; x < 44; ++x)
            for (int z = 28; z < 34; ++z)
                world.set_block(ORIGIN_X + x, 1, ORIGIN_Z + z, fluid_blocks.plant);
    }

    void reset_scene(sl::VoxelWorld &world, sl::FluidSimulation3D *sim, const sl::FluidVoxelBlocks &blocks)
    {
        // Wipe the whole box (fluid and terrain), then rebuild it.
        for (int y = 0; y < GRID_H; ++y)
            for (int z = 0; z < GRID_D; ++z)
                for (int x = 0; x < GRID_W; ++x)
                    if (world.get_block(ORIGIN_X + x, y, ORIGIN_Z + z) != sl::BLOCK_AIR)
                        world.set_block(ORIGIN_X + x, y, ORIGIN_Z + z, sl::BLOCK_AIR);
        build_scene(world, blocks);
        sl::fluid3d_import_from_voxels(sim, world, blocks);

        // Start with some water on the terrace and a sand heap so the first frames show something flowing.
        for (int y = 10; y < 15; ++y)
            for (int z = 18; z < 26; ++z)
                for (int x = 3; x < 9; ++x)
                    sl::fluid3d_set_cell(sim, x, y, z, sl::FluidElement::water, 1.0f);
        for (int y = 2; y < 6; ++y)
            for (int z = 8; z < 12; ++z)
                for (int x = 22; x < 26; ++x)
                    sl::fluid3d_set_cell(sim, x, y, z, sl::FluidElement::sand);
    }
} // namespace

int main(int argc, char **argv)
{
    if (!sl::configure_graphics_backend_from_args(argc, argv))
    {
        return -1;
    }
    constexpr int screen_w = 960;
    constexpr int screen_h = 600;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
    {
        std::fprintf(stderr, "Failed to initialize display: %s\n", sl::last_error().c_str());
        return -1;
    }
    sl::set_window_title("simlib - 3D Fluids in a Voxel World (exfluid3d)");

    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 1);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    const sl::FluidVoxelBlocks fluid_blocks;
    register_terrain_blocks();
    sl::fluid3d_register_blocks(fluid_blocks);

    sl::VoxelWorld world;
    sl::FluidSimulation3D *sim = sl::create_fluid_simulation_3d(GRID_W, GRID_H, GRID_D);
    sl::fluid3d_set_origin(sim, ORIGIN_X, 0, ORIGIN_Z);
    reset_scene(world, sim, fluid_blocks);
    world.update_dirty_chunks();

    sl::Material material;
    material.lit = true;
    material.cull = sl::CullMode::none;

    const glm::vec3 centre(0.0f, 8.0f, 0.0f);
    float yaw = 0.9f, pitch = 0.75f, distance = 85.0f;
    int brush = 0;
    bool paused = false;
    bool running = true;
    std::uint32_t last_time = sl::time_ms();

    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const float dt = std::clamp(static_cast<float>(now - last_time) * 0.001f, 0.001f, 0.1f);
        last_time = now;

        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            else if (event.type() == sl::Event::Type::key_down)
            {
                const auto key = event.key();
                if (key == sl::Event::Key::escape)
                    running = false;
                else if (key == sl::Event::Key::space)
                    paused = !paused;
                else if (key == sl::Event::Key::letter_r)
                    reset_scene(world, sim, fluid_blocks);
                const sl::Event::Key digits[] = {sl::Event::Key::digit_1, sl::Event::Key::digit_2, sl::Event::Key::digit_3,
                                                 sl::Event::Key::digit_4, sl::Event::Key::digit_5, sl::Event::Key::digit_6,
                                                 sl::Event::Key::digit_7, sl::Event::Key::digit_8, sl::Event::Key::digit_9,
                                                 sl::Event::Key::digit_0};
                for (int i = 0; i < BRUSH_COUNT; ++i)
                    if (key == digits[i])
                        brush = i;
            }
            sl::display_handle_event(event);
        }

        if (sl::key_down(sl::Key::letter_a))
            yaw -= 1.5f * dt;
        if (sl::key_down(sl::Key::letter_d))
            yaw += 1.5f * dt;
        if (sl::key_down(sl::Key::letter_w))
            pitch = std::min(1.4f, pitch + 1.0f * dt);
        if (sl::key_down(sl::Key::letter_s))
            pitch = std::max(0.1f, pitch - 1.0f * dt);
        if (sl::key_down(sl::Key::letter_q))
            distance = std::max(20.0f, distance - 40.0f * dt);
        if (sl::key_down(sl::Key::letter_e))
            distance = std::min(140.0f, distance + 40.0f * dt);

        const glm::vec3 eye = centre + distance * glm::vec3(std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw));
        const glm::vec3 forward = glm::normalize(centre - eye);
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up = glm::cross(right, forward);
        constexpr float fov = 55.0f;

        // Mouse ray through the pixel under the cursor.
        const float tan_half = std::tan(glm::radians(fov) * 0.5f);
        const float ndc_x = (static_cast<float>(sl::mouse_x()) / screen_w * 2.0f - 1.0f) * tan_half * (static_cast<float>(screen_w) / screen_h);
        const float ndc_y = (1.0f - static_cast<float>(sl::mouse_y()) / screen_h * 2.0f) * tan_half;
        const glm::vec3 ray_dir = glm::normalize(forward + right * ndc_x + up * ndc_y);
        const sl::VoxelRaycastResult hit = world.raycast(eye, ray_dir, 200.0f);

        if (hit.hit && sl::mouse_button_down(sl::MouseButton::left))
        {
            const Brush &b = BRUSHES[brush];
            for (int dz = 0; dz < 2; ++dz)
                for (int dx = 0; dx < 2; ++dx)
                {
                    const int gx = hit.place_block.x - ORIGIN_X + dx;
                    const int gy = hit.place_block.y + 1;
                    const int gz = hit.place_block.z - ORIGIN_Z + dz;
                    if (gx > 0 && gy > 0 && gz > 0 && gx < GRID_W - 1 && gy < GRID_H - 1 && gz < GRID_D - 1 &&
                        sl::fluid3d_get_cell(sim, gx, gy, gz).type == sl::FluidElement::empty)
                        sl::fluid3d_set_cell(sim, gx, gy, gz, b.element, 1.0f, b.temp, b.life);
                }
        }

        if (!paused)
            sl::fluid3d_step(sim);
        const std::size_t changed = sl::fluid3d_export_to_voxels(sim, world, fluid_blocks);
        world.update_dirty_chunks();

        sl::Camera camera;
        camera.position = eye;
        camera.target = centre;
        camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
        camera.field_of_view = fov;
        camera.near_plane = 0.5f;
        camera.far_plane = 400.0f;
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({18, 22, 36, 255});
            world.draw(material);
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {18, 22, 36});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);

        sl::gprintf(16, 12, {240, 240, 240}, "exfluid3d - 3D fluid simulation on a voxel world%s", paused ? "  [PAUSED]" : "");
        sl::gprintf(16, 30, {200, 220, 255}, "Brush: %s   Cells: %zu   Voxels changed: %zu   Triangles: %d", BRUSHES[brush].name,
                    sl::fluid3d_count_non_empty(sim), changed, world.total_triangles());
        sl::gprintf_center(screen_h - 22, {200, 210, 220},
                           "[L-Click] Spray  [1-9,0] Element  [Space] Pause  [R] Reset  [A/D] Orbit  [W/S] Tilt  [Q/E] Zoom  [Esc] Exit");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::destroy_fluid_simulation_3d(sim);
    sl::shutdown();
    return 0;
}
