#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 1100, 680))
    {
        std::fprintf(stderr, "Unable to start terrain showcase: %s\n", sl::last_error().c_str());
        return -1;
    }
    sl::set_window_title("simlib showcase - living world");
    sl::Surface3D *surface = sl::create_surface_3d(1100, 680, 1);
    if (!surface)
    {
        std::fprintf(stderr, "Unable to create 3D surface: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    sl::NoiseParameters noise;
    noise.seed = 24;
    noise.frequency = 1.0 / 2200.0;
    noise.amplitude = 90.0;
    noise.octaves = 4;
    noise.gain = 0.45;
    sl::TerrainConfig config;
    config.size = 8192.0;
    config.origin = {-4096.0, 0.0, -4096.0};
    config.finest_cell_size = 4.0;
    config.max_screen_error = 4.0;
    std::unique_ptr<sl::Terrain> terrain;
    std::unique_ptr<sl::ClutterLayer> clutter;
    try
    {
        terrain = std::make_unique<sl::Terrain>(config, std::make_shared<sl::NoiseSource>(noise));
        clutter = std::make_unique<sl::ClutterLayer>(*terrain);
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "Unable to create terrain showcase world: %s\n", error.what());
        sl::destroy_surface_3d(surface);
        sl::shutdown();
        return -1;
    }
    sl::ClutterSpecies trees;
    trees.mesh = sl::make_conifer_data(8.0f, 1.8f);
    trees.density = 0.0025f;
    trees.max_height = 100.0f;
    trees.max_slope = 32.0f;
    trees.near_distance = 180.0f;
    trees.far_distance = 1100.0f;
    if (clutter->add_species(trees) < 0)
    {
        std::fprintf(stderr, "Unable to bake terrain clutter: %s\n", sl::last_error().c_str());
        clutter.reset();
        terrain.reset();
        sl::destroy_surface_3d(surface);
        sl::shutdown();
        return -1;
    }

    sl::dvec3 position{0.0, 190.0, 900.0};
    double yaw = 0.0;
    double pitch = -0.15;
    double speed = 65.0;
    bool relative_mouse_captured = false;
    bool show_atmosphere = true;
    bool show_clouds = true;
    bool show_trees = true;
    bool running = true;
    std::uint32_t last_time = sl::time_ms();
    sl::set_fps(60);

    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const double dt = std::clamp(static_cast<double>(now - last_time) * 0.001, 0.001, 0.1);
        last_time = now;
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            else if (event.type() == sl::Event::Type::key_down)
            {
                if (event.key() == sl::Event::Key::escape)
                    running = false;
                else if (event.key() == sl::Event::Key::letter_h)
                    show_atmosphere = !show_atmosphere;
                else if (event.key() == sl::Event::Key::letter_k)
                    show_clouds = !show_clouds;
                else if (event.key() == sl::Event::Key::letter_t)
                    show_trees = !show_trees;
                else if (event.key() == sl::Event::Key::digit_1)
                    speed = 25.0;
                else if (event.key() == sl::Event::Key::digit_2)
                    speed = 65.0;
                else if (event.key() == sl::Event::Key::digit_3)
                    speed = 180.0;
            }
            sl::display_handle_event(event);
        }

        const bool looking = sl::mouse_button_down(sl::MouseButton::right);
        if (looking != relative_mouse_captured)
        {
            if (!sl::set_relative_mouse_mode(looking))
                std::fprintf(stderr, "Unable to capture mouse for look controls: %s\n", sl::last_error().c_str());
            else
                relative_mouse_captured = looking;
        }
        int mouse_dx = 0, mouse_dy = 0;
        sl::mouse_delta(&mouse_dx, &mouse_dy);
        if (relative_mouse_captured)
        {
            yaw += mouse_dx * 0.004;
            pitch = std::clamp(pitch - mouse_dy * 0.004, -1.2, 1.2);
        }

        const sl::dvec3 forward{std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
        const sl::dvec3 right{std::cos(yaw), 0.0, std::sin(yaw)};
        sl::dvec3 movement{0.0};
        if (sl::key_down(sl::Key::letter_w))
            movement += forward;
        if (sl::key_down(sl::Key::letter_s))
            movement -= forward;
        if (sl::key_down(sl::Key::letter_d))
            movement += right;
        if (sl::key_down(sl::Key::letter_a))
            movement -= right;
        if (sl::key_down(sl::Key::space))
            movement.y += 1.0;
        if (sl::key_down(sl::Key::letter_c))
            movement.y -= 1.0;
        const double boost = sl::key_down(sl::Key::left_shift) ? 3.0 : 1.0;
        if (glm::length(movement) > 0.0)
            position += glm::normalize(movement) * speed * boost * dt;
        position.y = std::max(position.y, terrain->height_at(position.x, position.z) + 5.0);

        sl::TerrainView view;
        view.position = position;
        view.forward = forward;
        view.up = {0.0, 1.0, 0.0};
        view.fov_y_degrees = 60.0;
        view.aspect = 1100.0 / 680.0;
        view.viewport_height = 680.0;
        view.far_distance = 18000.0;
        terrain->update(view);
        if (show_trees)
            clutter->update(view);

        sl::Camera camera;
        camera.position = {0.0f, 0.0f, 0.0f};
        camera.target = glm::vec3(forward);
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.field_of_view = 60.0f;
        camera.near_plane = 0.5f;
        camera.far_plane = static_cast<float>(view.far_distance);

        sl::Environment3D environment;
        environment.sun_direction = {-0.4f, -0.8f, 0.3f};
        environment.camera_altitude = static_cast<float>(position.y);
        environment.camera_position = position;
        environment.time = now * 0.001;
        environment.fog_density = 0.00008f;
        environment.clouds.enabled = show_clouds;
        environment.clouds.altitude = 1100.0f;
        environment.clouds.scale = 3500.0f;
        environment.clouds.coverage = 0.55f;

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({130, 180, 225, 255});
            sl::Light3D light;
            light.direction = environment.sun_direction;
            light.ambient = 0.35f;
            if (show_atmosphere)
            {
                sl::set_environment_3d(environment);
                sl::draw_sky_3d();
                light.colour = sl::atmosphere_sun_colour(environment);
            }
            else
            {
                sl::clear_environment_3d();
            }
            sl::set_light_3d(light);
            terrain->draw(position);
            if (show_trees)
                clutter->draw(position);
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {130, 180, 225});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        const sl::TerrainStats stats = terrain->stats();
        sl::gprintf(14, 12, {16, 25, 38}, "LIVING WORLD   |   chunks %d   triangles %d   queued %d",
                    stats.visible_chunks, stats.visible_triangles, stats.pending_chunks);
        sl::gprintf(14, 34, {16, 25, 38}, "H atmosphere %s   K clouds %s   T trees %s   speed %g",
                    show_atmosphere ? "on" : "off", show_clouds ? "on" : "off", show_trees ? "on" : "off", speed);
        sl::gprintf_center(656, {16, 25, 38}, "WASD fly   Space/C rise or fall   Right mouse look   1-3 speed   Esc quit");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    clutter.reset();
    terrain.reset();
    if (relative_mouse_captured)
        sl::set_relative_mouse_mode(false);
    sl::destroy_surface_3d(surface);
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}
