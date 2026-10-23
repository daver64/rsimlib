#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// Many lights in a 3D scene: coloured point lights orbit a courtyard of pillars and spheres, and a spotlight sweeps the
// floor, all added to a dim moonlight directional light with set_local_lights_3d().
//   Up/Down: number of lights   P: PBR or Lambert   S: sun shadows   L: light shadows   Space: pause   Right-drag: look   [ / ]: zoom   Esc: exit
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
    sl::set_window_title("simlib - Many lights (exlights3d)");
    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 4);
    sl::Mesh *floor = sl::create_plane_mesh(60.0f, 60.0f, {150, 150, 155, 255});
    sl::Mesh *cube = sl::create_cube_mesh(1.0f);
    sl::Mesh *sphere = sl::create_sphere_mesh(1.0f, 40, 24);
    sl::ShadowMap3D *shadow_map = sl::create_shadow_map_3d(2048);
    sl::LocalShadowMap3D *local_shadows = sl::create_local_shadow_map_3d(512, 2);
    if (!surface || !floor || !cube || !sphere || !shadow_map)
    {
        std::fprintf(stderr, "3D unavailable: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    float yaw = 0.6f, pitch = 0.45f, distance = 26.0f;
    int light_count = 10;
    bool pbr = true, shadows = true, light_shadows = true, paused = false, running = true;
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
                else if (key == sl::Event::Key::letter_p)
                    pbr = !pbr;
                else if (key == sl::Event::Key::letter_l)
                    light_shadows = !light_shadows;
                else if (key == sl::Event::Key::letter_s)
                    shadows = !shadows;
                else if (key == sl::Event::Key::space)
                    paused = !paused;
                else if (key == sl::Event::Key::arrow_up)
                    light_count = std::min(light_count + 1, sl::max_local_lights_3d);
                else if (key == sl::Event::Key::arrow_down)
                    light_count = std::max(light_count - 1, 0);
            }
            sl::display_handle_event(event);
        }
        if (sl::key_down(sl::Key::left_bracket))
            distance = std::max(8.0f, distance - 15.0f * static_cast<float>(frame));
        if (sl::key_down(sl::Key::right_bracket))
            distance = std::min(60.0f, distance + 15.0f * static_cast<float>(frame));
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
        camera.target = {0.0f, 1.5f, 0.0f};
        camera.position = camera.target + distance * glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
        camera.near_plane = 0.3f;
        camera.far_plane = 200.0f;

        // Lights: a spotlight first, then coloured point lights on circles of different radius and speed.
        std::vector<sl::LocalLight3D> lights;
        for (int i = 0; i < light_count; ++i)
        {
            sl::LocalLight3D light;
            if (i == 0)
            {
                const float a = static_cast<float>(clock) * 0.4f;
                light.position = {0.0f, 12.0f, 0.0f};
                light.direction = glm::normalize(glm::vec3(std::cos(a) * 9.0f, -12.0f, std::sin(a) * 9.0f));
                light.colour = {1.0f, 0.95f, 0.8f};
                light.intensity = 300.0f;
                light.range = 40.0f;
                light.inner_angle = 10.0f;
                light.outer_angle = 24.0f;
            }
            else
            {
                const float hue = static_cast<float>(i) / static_cast<float>(sl::max_local_lights_3d);
                const float a = static_cast<float>(clock) * (0.3f + 0.08f * i) * (i % 2 ? 1.0f : -1.0f) + i * 2.1f;
                const float radius = 5.0f + 1.2f * (i % 5);
                light.position = {std::cos(a) * radius, 1.4f + 0.8f * std::sin(a * 1.7f + i), std::sin(a) * radius};
                light.colour = {0.55f + 0.45f * std::sin(hue * 6.28f), 0.55f + 0.45f * std::sin(hue * 6.28f + 2.1f),
                                0.55f + 0.45f * std::sin(hue * 6.28f + 4.2f)};
                light.intensity = 6.0f;
                light.range = 11.0f;
            }
            light.cast_shadow = i < 2;
            lights.push_back(light);
        }

        sl::Light3D moon;
        moon.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.5f));
        moon.colour = {0.5f, 0.58f, 0.8f};
        moon.ambient = 0.06f;
        sl::set_light_3d(moon);
        sl::set_local_lights_3d(lights);

        auto draw_scene = [&]()
        {
        sl::Material material;
        material.pbr = pbr;
        material.metallic = 0.0f;
        material.roughness = 0.55f;
        sl::draw_mesh(floor, glm::mat4(1.0f), material);
        for (int i = 0; i < 8; ++i)
        {
            const float a = i * 0.7853982f + 0.4f;
            material.tint = {200, 190, 175, 255};
            sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), glm::vec3(std::cos(a) * 9.5f, 2.5f, std::sin(a) * 9.5f)) *
                                    glm::scale(glm::mat4(1.0f), glm::vec3(1.4f, 5.0f, 1.4f)),
                          material);
        }
        const sl::Colour tints[] = {{220, 60, 50, 255}, {60, 150, 220, 255}, {230, 200, 70, 255}, {90, 200, 110, 255}};
        for (int i = 0; i < 4; ++i)
        {
            const float a = i * 1.5707963f;
            material.tint = tints[i];
            material.metallic = i % 2 ? 1.0f : 0.0f;
            material.roughness = i % 2 ? 0.3f : 0.5f;
            sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), glm::vec3(std::cos(a) * 3.0f, 1.2f, std::sin(a) * 3.0f)) *
                                      glm::scale(glm::mat4(1.0f), glm::vec3(1.2f)),
                          material);
        }
        // The light positions themselves, drawn as small glowing spheres.
        sl::Material glow;
        glow.lit = false;
        for (std::size_t i = 1; i < lights.size(); ++i)
        {
            glow.tint = {static_cast<std::uint8_t>(255 * lights[i].colour.r), static_cast<std::uint8_t>(255 * lights[i].colour.g),
                         static_cast<std::uint8_t>(255 * lights[i].colour.b), 255};
            sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), lights[i].position) * glm::scale(glm::mat4(1.0f), glm::vec3(0.18f)), glow);
        }
        };

        sl::set_local_shadow_map_3d(nullptr);
        if (light_shadows && local_shadows && sl::render_local_shadows_3d(local_shadows, draw_scene))
            sl::set_local_shadow_map_3d(local_shadows);
        sl::set_shadow_map_3d(nullptr);
        if (shadows && sl::begin_shadow_pass_3d(shadow_map, {0.0f, 0.0f, 0.0f}, 16.0f))
        {
            draw_scene();
            sl::end_shadow_pass_3d();
            sl::set_shadow_map_3d(shadow_map);
        }
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({6, 8, 16, 255});
            draw_scene();
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        sl::gprintf(16, 12, {255, 255, 255}, "exlights3d - %d local lights   %s%s%s%s", light_count, pbr ? "PBR" : "Lambert", shadows ? "   shadows" : "", light_shadows ? "   light shadows" : "",
                    paused ? "   (paused)" : "");
        sl::gprintf_center(screen_h - 22, {235, 240, 250}, "[Up/Down] Lights  [P] PBR/Lambert  [S] Sun shadows  [L] Light shadows  [Space] Pause  [R-drag] Look  [ [ ] ] Zoom");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }
    sl::destroy_mesh(floor);
    sl::destroy_mesh(cube);
    sl::destroy_mesh(sphere);
    sl::destroy_local_shadow_map_3d(local_shadows);
    sl::destroy_shadow_map_3d(shadow_map);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
