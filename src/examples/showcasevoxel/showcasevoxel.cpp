#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr sl::BlockId grass = 1;
    constexpr sl::BlockId soil = 2;
    constexpr sl::BlockId stone = 3;
    constexpr sl::BlockId crystal = 4;

    void define_blocks()
    {
        sl::BlockDefinition definition;
        definition.tint = {90, 185, 75, 255};
        sl::register_block_definition(grass, definition);
        definition.tint = {145, 100, 65, 255};
        sl::register_block_definition(soil, definition);
        definition.tint = {125, 140, 165, 255};
        sl::register_block_definition(stone, definition);
        definition.tint = {95, 210, 255, 255};
        definition.light_emission = 12;
        definition.light_colour = {75, 195, 255, 255};
        sl::register_block_definition(crystal, definition);
    }

    void build_world(sl::VoxelWorld &world)
    {
        for (int z = -20; z <= 20; ++z)
        {
            for (int x = -20; x <= 20; ++x)
            {
                const double radius = std::sqrt(static_cast<double>(x * x + z * z));
                if (radius > 22.0)
                    continue;
                const int height = 3 + static_cast<int>(2.5 * std::sin(x * 0.25) + 2.5 * std::cos(z * 0.2));
                for (int y = -4; y <= height; ++y)
                {
                    const sl::BlockId id = y == height ? grass : (y >= height - 2 ? soil : stone);
                    world.set_block(x, y, z, id);
                }
            }
        }
        for (const glm::ivec3 position : {glm::ivec3(-5, 5, -4), glm::ivec3(5, 4, -8), glm::ivec3(8, 6, 5)})
            world.set_block(position.x, position.y, position.z, crystal);
    }
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 1000, 650))
    {
        std::fprintf(stderr, "Unable to start voxel showcase: %s\n", sl::last_error().c_str());
        return -1;
    }
    sl::set_window_title("simlib showcase - voxel workshop");
    sl::Surface3D *surface = sl::create_surface_3d(1000, 650, 1);
    if (!surface)
    {
        std::fprintf(stderr, "Unable to create 3D surface: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }
    define_blocks();
    sl::VoxelWorld world;
    build_world(world);
    world.update_dirty_chunks();

    glm::vec3 eye{0.0f, 11.0f, 30.0f};
    float yaw = 0.0f;
    float pitch = -0.18f;
    sl::BlockId selected_block = stone;
    bool running = true;
    std::uint32_t last_time = sl::time_ms();
    sl::set_relative_mouse_mode(true);
    sl::set_fps(60);

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
                switch (event.key())
                {
                case sl::Event::Key::escape:
                    running = false;
                    break;
                case sl::Event::Key::digit_1:
                    selected_block = grass;
                    break;
                case sl::Event::Key::digit_2:
                    selected_block = soil;
                    break;
                case sl::Event::Key::digit_3:
                    selected_block = stone;
                    break;
                case sl::Event::Key::digit_4:
                    selected_block = crystal;
                    break;
                case sl::Event::Key::letter_x:
                {
                    const glm::vec3 forward{std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                                            -std::cos(yaw) * std::cos(pitch)};
                    const sl::VoxelRaycastResult hit = world.raycast(eye, forward, 8.0f);
                    if (hit.hit)
                    {
                        world.set_block(hit.hit_block.x, hit.hit_block.y, hit.hit_block.z, sl::BLOCK_AIR);
                        world.update_dirty_chunks();
                    }
                    break;
                }
                case sl::Event::Key::letter_z:
                {
                    const glm::vec3 forward{std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                                            -std::cos(yaw) * std::cos(pitch)};
                    const sl::VoxelRaycastResult hit = world.raycast(eye, forward, 8.0f);
                    if (hit.hit && world.get_block(hit.place_block.x, hit.place_block.y, hit.place_block.z) == sl::BLOCK_AIR)
                    {
                        world.set_block(hit.place_block.x, hit.place_block.y, hit.place_block.z, selected_block);
                        world.update_dirty_chunks();
                    }
                    break;
                }
                default:
                    break;
                }
            }
            sl::display_handle_event(event);
        }

        int mouse_dx = 0, mouse_dy = 0;
        sl::mouse_delta(&mouse_dx, &mouse_dy);
        yaw += static_cast<float>(mouse_dx) * 0.003f;
        pitch = std::clamp(pitch - static_cast<float>(mouse_dy) * 0.003f, -1.35f, 1.35f);
        const glm::vec3 forward{std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                                -std::cos(yaw) * std::cos(pitch)};
        const glm::vec3 right{std::cos(yaw), 0.0f, std::sin(yaw)};
        glm::vec3 movement{0.0f};
        if (sl::key_down(sl::Key::letter_w))
            movement += forward;
        if (sl::key_down(sl::Key::letter_s))
            movement -= forward;
        if (sl::key_down(sl::Key::letter_d))
            movement += right;
        if (sl::key_down(sl::Key::letter_a))
            movement -= right;
        if (sl::key_down(sl::Key::space))
            movement.y += 1.0f;
        if (sl::key_down(sl::Key::left_control))
            movement.y -= 1.0f;
        if (glm::length(movement) > 0.0f)
            eye += glm::normalize(movement) * (sl::key_down(sl::Key::left_shift) ? 18.0f : 7.0f) * dt;

        sl::Camera camera;
        camera.position = eye;
        camera.target = eye + forward;
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.field_of_view = 70.0f;
        camera.near_plane = 0.1f;
        camera.far_plane = 150.0f;
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({20, 29, 48, 255});
            sl::Material material;
            material.lit = false;
            world.draw(material);
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {20, 29, 48});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        const char *block_name = selected_block == grass ? "grass" : selected_block == soil ? "soil" :
                                 selected_block == crystal ? "crystal" : "stone";
        sl::gprintf(14, 12, {235, 242, 255}, "VOXEL WORKSHOP   |   %zu chunks   %d triangles",
                    world.chunk_count(), world.total_triangles());
        sl::gprintf(14, 34, {235, 242, 255}, "Selected: %s   1 grass  2 soil  3 stone  4 crystal",
                    block_name);
        sl::gprintf_center(626, {235, 242, 255}, "WASD fly   Mouse look   X mine   Z place   Space/Ctrl rise/fall   Esc quit");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::set_relative_mouse_mode(false);
    sl::destroy_surface_3d(surface);
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}
