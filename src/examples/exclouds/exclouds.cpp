#include "sl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Cloud layers and types over a flat world or a planet. 1-6 toggle cirrus, altocumulus, cumulus, stratocumulus,
// stratus and cumulonimbus (up to four layers draw at once).
int main(int argc, char **argv)
{
    sl::set_text_panel_style({true});
    std::vector<char *> backend_arguments{argv[0]};
    std::string initial_types;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument(argv[i]);
        if (argument == "--gl" || argument == "--vulkan" || argument == "--d3d12")
            backend_arguments.push_back(argv[i]);
        else if (argument == "--types" && i + 1 < argc)
            initial_types = argv[++i];
        else
        {
            std::fprintf(stderr, "Usage: exclouds [--types cirrus,cumulus,...] [--gl|--vulkan|--d3d12]\n");
            return 2;
        }
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(backend_arguments.size()), backend_arguments.data()) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 640))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_window_title("simlib - cloud layers");
    sl::Surface3D *surface = sl::create_surface_3d(960, 640, 1);
    sl::Mesh *ground = sl::create_plane_mesh(60000.0f, 60000.0f, {70, 100, 60, 255});
    if (!surface || !ground)
    {
        std::fprintf(stderr, "3D unavailable: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    struct Entry
    {
        const char *name;
        sl::CloudType type;
        bool on;
    };
    std::array<Entry, 6> entries = {{{"cirrus", sl::CloudType::cirrus, true},
                                     {"altocumulus", sl::CloudType::altocumulus, false},
                                     {"cumulus", sl::CloudType::cumulus, true},
                                     {"stratocumulus", sl::CloudType::stratocumulus, false},
                                     {"stratus", sl::CloudType::stratus, false},
                                     {"cumulonimbus", sl::CloudType::cumulonimbus, true}}};
    if (!initial_types.empty())
        for (Entry &entry : entries)
            entry.on = ("," + initial_types + ",").find(std::string(",") + entry.name + ",") != std::string::npos;
    const sl::Event::Key keys[6] = {sl::Event::Key::digit_1, sl::Event::Key::digit_2, sl::Event::Key::digit_3,
                                    sl::Event::Key::digit_4, sl::Event::Key::digit_5, sl::Event::Key::digit_6};
    float yaw = 0.3f, pitch = 0.18f, altitude = 600.0f, sun_elevation = 0.45f;
    bool planet = false, volumetric = true;
    double time = 0.0;
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
                for (int i = 0; i < 6; ++i)
                    if (event.key() == keys[i])
                        entries[static_cast<std::size_t>(i)].on = !entries[static_cast<std::size_t>(i)].on;
                if (event.key() == sl::Event::Key::arrow_left)
                    yaw -= 0.1f;
                else if (event.key() == sl::Event::Key::arrow_right)
                    yaw += 0.1f;
                else if (event.key() == sl::Event::Key::arrow_up)
                    pitch = std::min(1.4f, pitch + 0.05f);
                else if (event.key() == sl::Event::Key::arrow_down)
                    pitch = std::max(-0.3f, pitch - 0.05f);
                else if (event.key() == sl::Event::Key::equals || event.key() == sl::Event::Key::plus)
                    altitude = std::min(20000.0f, altitude * 1.25f);
                else if (event.key() == sl::Event::Key::minus)
                    altitude = std::max(50.0f, altitude / 1.25f);
                else if (event.key() == sl::Event::Key::letter_t)
                    sun_elevation = sun_elevation > 0.9f ? 0.05f : sun_elevation + 0.2f;
                else if (event.key() == sl::Event::Key::letter_p)
                    planet = !planet;
                else if (event.key() == sl::Event::Key::letter_v)
                    volumetric = !volumetric;
            }
            sl::display_handle_event(event);
        }
        time += 1.0 / 60.0;

        sl::Environment3D environment;
        environment.sun_direction = glm::normalize(glm::vec3(-0.5f, -sun_elevation, 0.4f));
        environment.camera_altitude = altitude;
        environment.camera_position = {0.0, altitude, 0.0};
        environment.time = time * 8.0;
        environment.fog_density = 0.00004f;
        if (planet)
        {
            environment.planet = true;
            environment.sun_intensity = 20.0f;
        }
        int layers = 0;
        for (const Entry &entry : entries)
        {
            if (!entry.on || layers >= 4)
                continue;
            sl::CloudLayer layer = sl::cloud_layer(entry.type);
            // With volumetric off, the 3D types fall back to their flat layer.
            if (!volumetric)
                layer.volumetric = false;
            (layers == 0 ? environment.clouds : environment.more_clouds[layers - 1]) = layer;
            ++layers;
        }

        sl::Camera camera;
        camera.position = {0.0f, altitude, 0.0f};
        camera.target = camera.position + glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                                                    -std::cos(yaw) * std::cos(pitch));
        camera.field_of_view = 70.0f;
        camera.near_plane = 1.0f;
        camera.far_plane = 80000.0f;
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({130, 180, 225, 255});
            sl::set_environment_3d(environment);
            sl::draw_sky_3d();
            sl::Light3D light;
            light.direction = environment.sun_direction;
            light.colour = sl::atmosphere_sun_colour(environment);
            light.ambient = 0.35f;
            sl::set_light_3d(light);
            if (!planet)
                sl::draw_mesh(ground, glm::mat4(1.0f));
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {18, 22, 32});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        std::string line = "1-6:";
        for (const Entry &entry : entries)
            line += std::string(" ") + (entry.on ? "[" : " ") + entry.name + (entry.on ? "]" : " ");
        sl::gprintf(12, 12, {20, 30, 45}, "%s", line.c_str());
        sl::gprintf(12, 32, {20, 30, 45}, "Arrows look  =/-: altitude %.0fm  T: sun  P: planet %s  V: volumetric %s  Esc: exit",
                    altitude, planet ? "on" : "off", volumetric ? "on" : "off");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();

        ++frame;
        const char *frames = std::getenv("RSIMLIB_FRAMES");
        if (frames && frame >= static_cast<std::uint32_t>(std::max(1, std::atoi(frames))))
            running = false;
        const char *screenshot = std::getenv("RSIMLIB_SCREENSHOT");
        if (screenshot && frame == 2u && !sl::save_bitmap(sl::screen, screenshot))
            std::fprintf(stderr, "Screenshot save failed: %s\n", sl::last_error().c_str());
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(ground);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
