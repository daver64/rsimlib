#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Streams a Celestia virtual texture (colour, optional specular and normal) onto a sphere with sl::PlanetTiles.
// The data is not part of simlib: pass the paths of the .ctx files or tile folders.
// Arrows: move over the surface  =/-: altitude  L: rotate the sun  Esc: exit
int main(int argc, char **argv)
{
    sl::PlanetTilesOptions options;
    std::vector<char *> backend_arguments{argv[0]};
    double latitude = 20.0, longitude = 0.0, altitude_km = 12000.0;
    float sun_angle = 0.0f;
    constexpr double radius_km = 6371.0;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a(argv[i]);
        const bool has_value = i + 1 < argc;
        if (a == "--gl" || a == "--vulkan" || a == "--d3d12")
            backend_arguments.push_back(argv[i]);
        else if (a == "--colour" && has_value)
            options.colour = argv[++i];
        else if (a == "--specular" && has_value)
            options.specular = argv[++i];
        else if (a == "--normal" && has_value)
            options.normal = argv[++i];
        else if (a == "--lat" && has_value)
            latitude = std::atof(argv[++i]);
        else if (a == "--lon" && has_value)
            longitude = std::atof(argv[++i]);
        else if (a == "--alt" && has_value)
            altitude_km = std::atof(argv[++i]);
        else if (a == "--sun" && has_value)
            sun_angle = static_cast<float>(std::atof(argv[++i]));
        else
        {
            std::fprintf(stderr,
                         "Usage: exearth --colour <.ctx|folder> [--specular <.ctx|folder>] [--normal <.ctx|folder>]\n"
                         "               [--lat deg] [--lon deg] [--alt km] [--sun radians] [--gl|--vulkan|--d3d12]\n");
            return 2;
        }
    }
    if (options.colour.empty())
    {
        std::fprintf(stderr, "exearth needs --colour <.ctx or tile folder> (data is not shipped with simlib)\n");
        return 2;
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(backend_arguments.size()), backend_arguments.data()) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 1280, 720))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_window_title("simlib - tiled planet");
    sl::Surface3D *surface = sl::create_surface_3d(1280, 720, 4);
    auto planet = sl::PlanetTiles::create(options);
    if (!surface || !planet)
    {
        std::fprintf(stderr, "Setup failed: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    const double pi = 3.14159265358979323846;
    bool running = true;
    std::uint32_t frame = 0;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down)
            {
                const double step = std::clamp(altitude_km / radius_km, 0.0005, 1.0) * 10.0;
                if (event.key() == sl::Event::Key::arrow_left)
                    longitude -= step;
                else if (event.key() == sl::Event::Key::arrow_right)
                    longitude += step;
                else if (event.key() == sl::Event::Key::arrow_up)
                    latitude = std::min(89.0, latitude + step);
                else if (event.key() == sl::Event::Key::arrow_down)
                    latitude = std::max(-89.0, latitude - step);
                else if (event.key() == sl::Event::Key::plus || event.key() == sl::Event::Key::equals ||
                         event.key() == sl::Event::Key::keypad_plus)
                    altitude_km = std::max(0.5, altitude_km / 1.3);
                else if (event.key() == sl::Event::Key::minus || event.key() == sl::Event::Key::keypad_minus)
                    altitude_km = std::min(60000.0, altitude_km * 1.3);
                else if (event.key() == sl::Event::Key::letter_l)
                    sun_angle += 0.25f;
            }
            sl::display_handle_event(event);
        }

        // Planet radius is 1 unit. The camera looks straight down at the surface point.
        const double lat = latitude * pi / 180.0, lon = longitude * pi / 180.0;
        // Tile column 0 starts at longitude -180 on the +X axis, so longitude 0 faces -X and east is +Z.
        const glm::vec3 up(static_cast<float>(-std::cos(lat) * std::cos(lon)), static_cast<float>(std::sin(lat)),
                           static_cast<float>(std::cos(lat) * std::sin(lon)));
        const float height = static_cast<float>(1.0 + altitude_km / radius_km);
        sl::Camera camera;
        camera.position = up * height;
        camera.target = up;
        const glm::vec3 north(0.0f, 1.0f, 0.0f);
        camera.up = std::abs(up.y) > 0.999f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::normalize(north - up * up.y);
        camera.near_plane = std::max(0.00001f, (height - 1.0f) * 0.1f);
        camera.far_plane = 10.0f;
        camera.field_of_view = 40.0f;
        const glm::mat4 view_projection = camera.projection_matrix(1280.0f / 720.0f) * camera.view_matrix();
        planet->update(glm::dvec3(camera.position), glm::radians(camera.field_of_view), 720.0f, &view_projection);

        sl::Light3D sun;
        sun.direction = -glm::normalize(glm::vec3(-std::cos(sun_angle), 0.3f, std::sin(sun_angle)));
        sun.ambient = 0.05f;
        sl::set_light_3d(sun);
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({0, 0, 0, 255});
            sl::Material material;
            material.lit = true;
            material.fog = false;
            planet->draw(glm::mat4(1.0f), material);
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        const sl::PlanetTilesStats stats = planet->stats();
        sl::gprintf(16, 16, {255, 255, 255}, "lat %.3f lon %.3f alt %.2f km  level %d/%d  patches %d  tiles %d (loading %d)",
                    latitude, longitude, altitude_km, stats.deepest_level, planet->deepest_level(), stats.patches,
                    stats.tiles_loaded, stats.tiles_loading);
        sl::gprintf(16, 36, {200, 210, 225}, "Arrows: move  =/-: altitude  L: sun  Esc: exit%s",
                    planet->has_specular() ? "  (specular on)" : "");
        sl::show_video_bitmap();
        sl::end_frame();

        ++frame;
        const char *frames = std::getenv("RSIMLIB_FRAMES");
        const char *screenshot = std::getenv("RSIMLIB_SCREENSHOT");
        // With a screenshot requested, wait until streaming has settled so the shot shows the full detail.
        if (screenshot && frame >= 30u && stats.tiles_loading == 0 && frame % 10u == 0u)
        {
            if (!sl::save_bitmap(sl::screen, screenshot))
                std::fprintf(stderr, "Screenshot save failed: %s\n", sl::last_error().c_str());
            running = false;
        }
        if (frames && frame >= static_cast<std::uint32_t>(std::max(1, std::atoi(frames))))
            running = false;
    }

    sl::wait_for_graphics();
    planet.reset();
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
