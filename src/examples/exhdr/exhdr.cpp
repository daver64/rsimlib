#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// HDR post-process: a lamp-lit courtyard rendered through sl::Hdr. Bright lamps and lit highlights bloom in scene space and roll off
// through a tone-mapping curve instead of clipping, and exposure can be changed live.
//   H: HDR on/off   Up/Down: exposure   T: tone-map curve   Space: pause   Right-drag: look   [ / ]: zoom   Esc: exit
int main(int argc, char **argv)
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv))
        return 1;
    constexpr int screen_w = 1100;
    constexpr int screen_h = 680;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_window_title("simlib - HDR post-process (exhdr)");
    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 4);
    sl::Mesh *floor = sl::create_plane_mesh(60.0f, 60.0f, {120, 120, 128, 255});
    sl::Mesh *cube = sl::create_cube_mesh(1.0f);
    sl::Mesh *sphere = sl::create_sphere_mesh(1.0f, 40, 24);
    sl::Hdr hdr;
    if (!surface || !floor || !cube || !sphere || !hdr.initialise())
    {
        std::fprintf(stderr, "HDR demo unavailable: %s\n", surface ? hdr.error().c_str() : sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    float yaw = 0.7f, pitch = 0.32f, distance = 24.0f, exposure = 1.0f;
    bool hdr_on = true, aces = false, paused = false, running = true;
    double clock = 0.0;
    int last_x = 0, last_y = 0;
    std::uint32_t last_time = sl::time_ms();
    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const double frame = std::clamp(static_cast<double>(now - last_time) * 0.001, 0.001, 0.1);
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
                else if (key == sl::Event::Key::letter_h)
                    hdr_on = !hdr_on;
                else if (key == sl::Event::Key::letter_t)
                    aces = !aces;
                else if (key == sl::Event::Key::space)
                    paused = !paused;
            }
            sl::display_handle_event(event);
        }
        if (sl::key_down(sl::Key::arrow_up))
            exposure = std::min(4.0f, exposure + 1.2f * static_cast<float>(frame));
        if (sl::key_down(sl::Key::arrow_down))
            exposure = std::max(0.2f, exposure - 1.2f * static_cast<float>(frame));
        if (sl::key_down(sl::Key::left_bracket))
            distance = std::max(8.0f, distance - 15.0f * static_cast<float>(frame));
        if (sl::key_down(sl::Key::right_bracket))
            distance = std::min(50.0f, distance + 15.0f * static_cast<float>(frame));
        const int mx = sl::mouse_x(), my = sl::mouse_y();
        if (sl::mouse_button_down(sl::MouseButton::right))
        {
            yaw -= static_cast<float>(mx - last_x) * 0.008f;
            pitch = std::clamp(pitch + static_cast<float>(my - last_y) * 0.008f, 0.08f, 1.45f);
        }
        last_x = mx;
        last_y = my;
        if (!paused)
            clock += frame;

        sl::Camera camera;
        camera.target = {0.0f, 2.0f, 0.0f};
        camera.position = camera.target + distance * glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
        camera.near_plane = 0.3f;
        camera.far_plane = 200.0f;

        // Six lamps on a ring plus one warm fire in the middle.
        struct Lamp
        {
            glm::vec3 position;
            glm::vec3 colour;
        };
        std::vector<Lamp> lamps;
        for (int i = 0; i < 6; ++i)
        {
            const float a = i * 1.0471976f + 0.3f;
            const glm::vec3 colour = i % 2 ? glm::vec3(0.55f, 0.75f, 1.0f) : glm::vec3(1.0f, 0.72f, 0.4f);
            lamps.push_back({{std::cos(a) * 9.0f, 4.2f, std::sin(a) * 9.0f}, colour});
        }
        lamps.push_back({{0.0f, 0.9f, 0.0f}, {1.0f, 0.5f, 0.2f}});

        std::vector<sl::LocalLight3D> lights;
        for (std::size_t i = 0; i < lamps.size(); ++i)
        {
            sl::LocalLight3D light;
            const float flicker = 1.0f + 0.12f * std::sin(static_cast<float>(clock) * (7.0f + 1.3f * static_cast<float>(i)) + static_cast<float>(i));
            light.position = lamps[i].position;
            light.colour = lamps[i].colour;
            light.intensity = (i + 1 == lamps.size() ? 14.0f : 24.0f) * flicker;
            light.range = 18.0f;
            lights.push_back(light);
        }
        sl::Light3D moon;
        moon.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.5f));
        moon.colour = {0.45f, 0.55f, 0.85f};
        moon.ambient = 0.05f;
        sl::set_light_3d(moon);
        sl::set_local_lights_3d(lights);

        hdr.set_bloom_threshold(2.0f);
        hdr.set_bloom_intensity(0.9f);
        hdr.set_exposure(exposure);
        hdr.set_tonemap(aces ? sl::HdrTonemap::aces : sl::HdrTonemap::reinhard);

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({5, 7, 14, 255});
            sl::Material material;
            material.metallic = 0.0f;
            material.roughness = 0.5f;
            sl::draw_mesh(floor, glm::mat4(1.0f), material);
            for (int i = 0; i < 8; ++i)
            {
                const float a = i * 0.7853982f + 0.4f;
                material.tint = {210, 200, 185, 255};
                sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), glm::vec3(std::cos(a) * 13.0f, 3.0f, std::sin(a) * 13.0f)) *
                                        glm::scale(glm::mat4(1.0f), glm::vec3(1.6f, 6.0f, 1.6f)),
                              material);
            }
            const sl::Colour tints[] = {{225, 70, 55, 255}, {70, 150, 225, 255}, {235, 205, 80, 255}, {230, 230, 235, 255}};
            for (int i = 0; i < 4; ++i)
            {
                const float a = i * 1.5707963f + 0.8f;
                material.tint = tints[i];
                material.metallic = i % 2 ? 1.0f : 0.0f;
                material.roughness = i % 2 ? 0.25f : 0.4f;
                sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), glm::vec3(std::cos(a) * 4.0f, 1.3f, std::sin(a) * 4.0f)) *
                                          glm::scale(glm::mat4(1.0f), glm::vec3(1.3f)),
                              material);
            }
            // Bulbs are unlit and near white, so HDR reads them as far brighter than page white.
            sl::Material bulb;
            bulb.lit = false;
            for (const Lamp &lamp : lamps)
            {
                const glm::vec3 core = glm::mix(lamp.colour, glm::vec3(1.0f), 0.65f);
                bulb.tint = {static_cast<std::uint8_t>(255 * core.r), static_cast<std::uint8_t>(255 * core.g), static_cast<std::uint8_t>(255 * core.b), 255};
                sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), lamp.position) * glm::scale(glm::mat4(1.0f), glm::vec3(0.55f)), bulb);
            }
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {0, 0, 0});
        if (hdr_on)
            hdr.apply(sl::surface_3d_bitmap(surface), 0, 0, screen_w, screen_h);
        else
            sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        sl::gprintf(16, 12, {255, 255, 255}, "exhdr - HDR %s   exposure %.2f   %s", hdr_on ? "on" : "off", exposure, aces ? "ACES" : "Reinhard");
        sl::gprintf_center(screen_h - 24, {200, 210, 230}, "[H] HDR  [Up/Down] Exposure  [T] Curve  [Space] Pause  [R-drag] Look  [ [ ] ] Zoom");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }
    sl::wait_for_graphics();
    hdr.shutdown();
    sl::destroy_mesh(floor);
    sl::destroy_mesh(cube);
    sl::destroy_mesh(sphere);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
