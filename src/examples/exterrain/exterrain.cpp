#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Chunked-LOD terrain. Fly over procedural fractal terrain: a flat arena (default) or a planet (run with "planet").
//
//   W/S/A/D: move   Space/C: up/down   Shift: fast   Right-drag: look
//   1-4: speed presets   R: new seed   Esc: exit

int main(int argc, char **argv)
{
    bool planet = false;
    std::vector<char *> args;
    for (int i = 0; i < argc; ++i)
    {
        if (i > 0 && std::strcmp(argv[i], "planet") == 0)
            planet = true;
        else
            args.push_back(argv[i]);
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(args.size()), args.data()))
    {
        std::fprintf(stderr, "Invalid graphics backend arguments: %s\n", sl::last_error().c_str());
        return -1;
    }

    constexpr int screen_w = 1100;
    constexpr int screen_h = 680;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
    {
        std::fprintf(stderr, "Failed to initialize display: %s\n", sl::last_error().c_str());
        return -1;
    }
    sl::set_window_title("simlib - Chunked LOD Terrain (exterrain)");
    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 1);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    int seed = 7;
    std::unique_ptr<sl::Terrain> terrain;
    auto build = [&]()
    {
        sl::NoiseParameters noise;
        noise.seed = seed;
        sl::TerrainConfig config;
        if (planet)
        {
            config.shape = sl::TerrainShape::sphere;
            config.radius = 6.371e6;
            noise.frequency = 1.0 / 400000.0;
            noise.amplitude = 6000.0;
            noise.floor_height = 0.0;
            config.finest_cell_size = 2.0;
            config.uv_tile_metres = 32.0;
        }
        else
        {
            config.size = 16384.0;
            config.origin = {-8192.0, 0.0, -8192.0};
            noise.frequency = 1.0 / 3000.0;
            noise.amplitude = 500.0;
            noise.ridged = true;
            config.finest_cell_size = 1.0;
        }
        terrain = std::make_unique<sl::Terrain>(config, std::make_shared<sl::NoiseSource>(noise));
    };
    build();

    sl::dvec3 position = planet ? sl::dvec3(0.0, 6.371e6 + 3000.0, 0.0) : sl::dvec3(0.0, 700.0, 0.0);
    double yaw = 0.0, pitch = -0.3;
    double speed = planet ? 400.0 : 80.0;
    int last_mouse_x = 0, last_mouse_y = 0;
    std::uint32_t last_time = sl::time_ms();
    bool running = true;

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
                const auto key = event.key();
                if (key == sl::Event::Key::escape)
                    running = false;
                else if (key == sl::Event::Key::letter_r)
                {
                    ++seed;
                    build();
                }
                else if (key == sl::Event::Key::digit_1)
                    speed = 20.0;
                else if (key == sl::Event::Key::digit_2)
                    speed = 100.0;
                else if (key == sl::Event::Key::digit_3)
                    speed = 500.0;
                else if (key == sl::Event::Key::digit_4)
                    speed = 5000.0;
            }
        }

        const int mx = sl::mouse_x(), my = sl::mouse_y();
        if (sl::mouse_button_down(sl::MouseButton::right))
        {
            yaw -= (mx - last_mouse_x) * 0.005;
            pitch = std::clamp(pitch - (my - last_mouse_y) * 0.005, -1.5, 1.5);
        }
        last_mouse_x = mx;
        last_mouse_y = my;

        // Local frame: "up" is world Y on the flat arena and radial above a planet.
        const sl::dvec3 up = planet ? glm::normalize(position) : sl::dvec3(0.0, 1.0, 0.0);
        const sl::dvec3 ref = std::fabs(up.y) < 0.99 ? sl::dvec3(0.0, 1.0, 0.0) : sl::dvec3(1.0, 0.0, 0.0);
        const sl::dvec3 east = glm::normalize(glm::cross(ref, up));
        const sl::dvec3 north = glm::cross(up, east);
        const sl::dvec3 heading = std::cos(yaw) * north + std::sin(yaw) * -east;
        const sl::dvec3 right = glm::cross(heading, up);
        const sl::dvec3 forward = std::cos(pitch) * heading + std::sin(pitch) * up;

        const double boost = sl::key_down(sl::Key::left_shift) ? 4.0 : 1.0;
        sl::dvec3 move(0.0);
        if (sl::key_down(sl::Key::letter_w))
            move += forward;
        if (sl::key_down(sl::Key::letter_s))
            move -= forward;
        if (sl::key_down(sl::Key::letter_d))
            move += right;
        if (sl::key_down(sl::Key::letter_a))
            move -= right;
        if (sl::key_down(sl::Key::space))
            move += up;
        if (sl::key_down(sl::Key::letter_c))
            move -= up;
        position += move * speed * boost * dt;
        const double altitude = terrain->altitude_of(position);
        if (altitude < 2.0)
            position += up * (2.0 - altitude);

        sl::TerrainView view;
        view.position = position;
        view.forward = forward;
        view.up = up;
        view.fov_y_degrees = 60.0;
        view.aspect = static_cast<double>(screen_w) / screen_h;
        view.viewport_height = screen_h;
        view.far_distance = planet ? 3.0e6 : 30000.0;
        terrain->update(view);

        // The camera sits at the origin and the terrain is drawn relative to the eye, so floats stay precise.
        sl::Camera camera;
        camera.position = glm::vec3(0.0f);
        camera.target = glm::vec3(forward);
        camera.up = glm::vec3(up);
        camera.field_of_view = 60.0f;
        camera.near_plane = 0.5f;
        camera.far_plane = static_cast<float>(view.far_distance);

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({150, 185, 230, 255});
            sl::Light3D light;
            light.direction = glm::normalize(glm::vec3(-0.5f, -0.8f, -0.3f));
            light.ambient = 0.3f;
            sl::set_light_3d(light);
            sl::Material material;
            terrain->draw(position, material);
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {150, 185, 230});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);

        const sl::TerrainStats stats = terrain->stats();
        sl::gprintf(12, 10, {20, 20, 20}, "exterrain - %s terrain   %d chunks, %d triangles, deepest level %d / %d",
                    planet ? "planet" : "flat", stats.visible_chunks, stats.visible_triangles, stats.deepest_level,
                    terrain->max_level());
        sl::gprintf(12, 28, {20, 20, 20}, "Altitude %.1f m   Speed %.0f m/s   Resident %d   Loading %d",
                    terrain->altitude_of(position), speed * boost, stats.resident_chunks, stats.pending_chunks);
        sl::gprintf_center(screen_h - 22, {20, 20, 20},
                           "[WASD] Move  [Space/C] Up/Down  [Shift] Fast  [1-4] Speed  [R-drag] Look  [R] New seed");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    terrain.reset();
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
