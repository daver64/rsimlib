#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// The low-poly props library (props3d.h): every part lit and shadowed on a ground plane under an orbiting camera.
//   Left/Right: focus one part   A: show all   N: day or night   Space: pause orbit   Right-drag: look   [ / ]: zoom   Esc: exit
namespace
{
    struct Prop
    {
        std::string name;
        sl::MeshData data;
        sl::Mesh *mesh = nullptr;
        glm::vec3 lo{0.0f}, hi{0.0f};
        glm::vec3 grid_position{0.0f};
        float extent() const { return std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}); }
    };

    Prop make_prop(const char *name, sl::MeshData data)
    {
        Prop prop;
        prop.name = name;
        prop.lo = glm::vec3{1e9f};
        prop.hi = glm::vec3{-1e9f};
        for (const sl::Vertex3D &v : data.vertices)
        {
            const glm::vec3 p{v.position[0], v.position[1], v.position[2]};
            prop.lo = glm::min(prop.lo, p);
            prop.hi = glm::max(prop.hi, p);
        }
        prop.mesh = sl::create_mesh(data.vertices, data.indices);
        prop.data = std::move(data);
        return prop;
    }
} // namespace

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
    sl::set_window_title("simlib - Props library (exprops)");
    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 4);
    sl::ShadowMap3D *shadow_map = sl::create_shadow_map_3d(4096);
    sl::Mesh *ground = sl::create_plane_mesh(1600.0f, 1600.0f, {128, 140, 108, 255});
    if (!surface || !shadow_map || !ground)
    {
        std::fprintf(stderr, "3D unavailable: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    std::vector<Prop> props;
    props.push_back(make_prop("house", sl::make_house_data()));
    props.push_back(make_prop("office", sl::make_office_data()));
    props.push_back(make_prop("skyscraper", sl::make_skyscraper_data()));
    props.push_back(make_prop("hangar", sl::make_hangar_data()));
    props.push_back(make_prop("wall", sl::make_wall_data()));
    props.push_back(make_prop("fence", sl::make_fence_data()));
    props.push_back(make_prop("water tower", sl::make_water_tower_data()));
    props.push_back(make_prop("radar", sl::make_radar_data()));
    props.push_back(make_prop("domed radar", sl::make_domed_radar_data()));
    props.push_back(make_prop("control tower", sl::make_control_tower_data()));
    props.push_back(make_prop("street lamp", sl::make_street_lamp_data()));
    props.push_back(make_prop("road", sl::make_road_data()));
    props.push_back(make_prop("barrel", sl::make_barrel_data()));
    props.push_back(make_prop("crate", sl::make_crate_data()));
    props.push_back(make_prop("container", sl::make_container_data()));
    props.push_back(make_prop("tank", sl::make_tank_data()));
    props.push_back(make_prop("tree", sl::make_tree_data()));
    props.push_back(make_prop("conifer", sl::make_conifer_data()));
    props.push_back(make_prop("bush", sl::make_bush_data()));
    props.push_back(make_prop("rock", sl::make_rock_data()));
    props.push_back(make_prop("rock cluster", sl::make_rock_cluster_data()));
    props.push_back(make_prop("car", sl::make_car_data()));
    props.push_back(make_prop("spaceship", sl::make_spaceship_data()));
    props.push_back(make_prop("aeroplane", sl::make_aeroplane_data()));

    // Lay the parts out in rows, each part centred in its own footprint.
    constexpr float row_width = 80.0f;
    float cursor_x = 0.0f, cursor_z = 0.0f, row_depth = 0.0f, grid_extent = 0.0f;
    for (Prop &prop : props)
    {
        const float w = std::max(prop.hi.x - prop.lo.x, 1.5f) + 4.0f, d = std::max(prop.hi.z - prop.lo.z, 1.5f) + 4.0f;
        if (cursor_x + w > row_width && cursor_x > 0.0f)
        {
            cursor_x = 0.0f;
            cursor_z += row_depth;
            row_depth = 0.0f;
        }
        prop.grid_position = {cursor_x + 0.5f * w, 0.0f, cursor_z + 0.5f * d};
        cursor_x += w;
        row_depth = std::max(row_depth, d);
        grid_extent = std::max(grid_extent, cursor_x);
    }
    const float grid_depth = cursor_z + row_depth;
    const glm::vec3 grid_centre{0.5f * grid_extent, 0.0f, 0.5f * grid_depth};
    for (Prop &prop : props)
        prop.grid_position -= grid_centre;

    int focus = -1; // -1 shows everything
    bool night = false, paused = false, running = true;
    float yaw = 0.6f, pitch = 0.5f, zoom = 1.0f;
    int last_x = 0, last_y = 0;
    std::uint32_t last_time = sl::time_ms();
    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const float frame = std::clamp(static_cast<float>(now - last_time) * 0.001f, 0.001f, 0.1f);
        last_time = now;
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            else if (event.type() == sl::Event::Type::key_down)
            {
                const auto key = event.key();
                const int count = static_cast<int>(props.size());
                if (key == sl::Event::Key::escape)
                    running = false;
                else if (key == sl::Event::Key::arrow_right)
                    focus = focus < 0 ? 0 : (focus + 1) % count;
                else if (key == sl::Event::Key::arrow_left)
                    focus = focus < 0 ? count - 1 : (focus + count - 1) % count;
                else if (key == sl::Event::Key::letter_a)
                    focus = -1;
                else if (key == sl::Event::Key::letter_n)
                    night = !night;
                else if (key == sl::Event::Key::space)
                    paused = !paused;
            }
            sl::display_handle_event(event);
        }
        if (sl::key_down(sl::Key::left_bracket))
            zoom = std::max(0.4f, zoom - 0.6f * frame);
        if (sl::key_down(sl::Key::right_bracket))
            zoom = std::min(2.5f, zoom + 0.6f * frame);
        const int mx = sl::mouse_x(), my = sl::mouse_y();
        if (sl::mouse_button_down(sl::MouseButton::right))
        {
            yaw -= static_cast<float>(mx - last_x) * 0.008f;
            pitch = std::clamp(pitch + static_cast<float>(my - last_y) * 0.008f, 0.08f, 1.45f);
        }
        else if (!paused)
            yaw += 0.18f * frame;
        last_x = mx;
        last_y = my;

        // Frame either the whole gallery or the focused part.
        glm::vec3 target{0.0f, 8.0f, 0.0f};
        float distance = 0.5f * std::max(grid_extent, grid_depth) * 2.1f * zoom, shadow_radius = 0.5f * std::max(grid_extent, grid_depth) * 1.25f;
        if (focus >= 0)
        {
            const Prop &prop = props[static_cast<std::size_t>(focus)];
            target = {0.0f, 0.5f * (prop.hi.y + prop.lo.y), 0.0f};
            distance = std::max(prop.extent() * 1.35f, 4.0f) * zoom;
            shadow_radius = std::max(prop.extent() * 1.2f, 6.0f);
        }
        sl::Camera camera;
        camera.target = target;
        camera.position = target + distance * glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
        camera.near_plane = 0.3f;
        camera.far_plane = 1600.0f;

        sl::Light3D sun;
        sun.direction = glm::normalize(night ? glm::vec3(-0.3f, -1.0f, -0.5f) : glm::vec3(-0.5f, -0.8f, -0.35f));
        sun.colour = night ? glm::vec3(0.35f, 0.42f, 0.7f) : glm::vec3(1.0f, 0.95f, 0.85f);
        sun.ambient = night ? 0.12f : 0.34f;
        sl::set_light_3d(sun);

        // At night a warm light hangs by the focused part, or by each small part in the overview.
        std::vector<sl::LocalLight3D> lights;
        if (night)
        {
            const auto add_light = [&](const glm::vec3 &position, float intensity, float range)
            {
                if (static_cast<int>(lights.size()) >= sl::max_local_lights_3d)
                    return;
                sl::LocalLight3D light;
                light.position = position;
                light.colour = {1.0f, 0.8f, 0.5f};
                light.intensity = intensity;
                light.range = range;
                lights.push_back(light);
            };
            if (focus >= 0)
            {
                const Prop &prop = props[static_cast<std::size_t>(focus)];
                const float scale = std::max(prop.extent() / 8.0f, 0.5f);
                add_light({prop.hi.x + 1.5f * scale, prop.hi.y * 0.6f + 1.0f, prop.hi.z + 1.5f * scale}, 40.0f * scale, 16.0f * scale);
                add_light({prop.lo.x - 1.5f * scale, prop.hi.y * 0.6f + 1.0f, prop.lo.z - 1.5f * scale}, 25.0f * scale, 16.0f * scale);
            }
            else
            {
                for (const Prop &prop : props)
                    if (prop.extent() < 12.0f)
                        add_light(prop.grid_position + glm::vec3(0.0f, 4.0f, 3.5f), 14.0f, 14.0f);
            }
        }
        sl::set_local_lights_3d(lights);

        auto draw_scene = [&]()
        {
            sl::Material material;
            material.pbr = false;
            sl::draw_mesh(ground, glm::mat4(1.0f), material);
            for (std::size_t i = 0; i < props.size(); ++i)
            {
                if (focus >= 0 && static_cast<int>(i) != focus)
                    continue;
                const glm::vec3 position = focus >= 0 ? glm::vec3(0.0f) : props[i].grid_position;
                sl::draw_mesh(props[i].mesh, glm::translate(glm::mat4(1.0f), position), material);
            }
        };

        sl::set_shadow_map_3d(nullptr);
        if (sl::begin_shadow_pass_3d(shadow_map, {0.0f, 0.0f, 0.0f}, shadow_radius * 1.4f))
        {
            draw_scene();
            sl::end_shadow_pass_3d();
            sl::set_shadow_map_3d(shadow_map);
        }
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d(night ? sl::Colour{8, 10, 22, 255} : sl::Colour{150, 186, 222, 255});
            draw_scene();
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        if (focus >= 0)
            sl::gprintf(16, 12, {255, 255, 255}, "exprops - %s   %zu triangles   %s", props[static_cast<std::size_t>(focus)].name.c_str(),
                        props[static_cast<std::size_t>(focus)].data.indices.size() / 3, night ? "night" : "day");
        else
            sl::gprintf(16, 12, {255, 255, 255}, "exprops - all %zu parts   %s", props.size(), night ? "night" : "day");
        sl::gprintf_center(screen_h - 22, {235, 240, 250}, "[</>] Focus a part  [A] All  [N] Day/night  [Space] Pause  [R-drag] Look  [ [ ] ] Zoom");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }
    for (Prop &prop : props)
        sl::destroy_mesh(prop.mesh);
    sl::destroy_mesh(ground);
    sl::destroy_shadow_map_3d(shadow_map);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
