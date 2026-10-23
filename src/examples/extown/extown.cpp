#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// A small town and airfield built entirely from the props library (props3d.h). Each kind of part is baked once with
// bake_instances(), so the whole scene is a couple of dozen draw calls.
//   1: town   2: airfield   N: day or night   Space: pause orbit   Right-drag: look   [ / ]: zoom   Esc: exit
namespace
{
    constexpr float pi = 3.14159265358979f;

    float hash01(std::uint32_t value)
    {
        value ^= value >> 16;
        value *= 0x7feb352dU;
        value ^= value >> 15;
        value *= 0x846ca68bU;
        value ^= value >> 16;
        return static_cast<float>(value & 0xffffffU) / 16777215.0f;
    }

    /** One kind of part with everything placed in the scene. Parts that vary by seed get several variants. */
    struct Kind
    {
        std::vector<sl::MeshData> variants;
        std::vector<std::vector<sl::Instance3D>> placed;
        std::vector<sl::Mesh *> meshes;

        explicit Kind(std::vector<sl::MeshData> data) : variants(std::move(data)), placed(variants.size()) {}
        void place(float x, float z, float yaw, float scale = 1.0f, sl::Colour tint = {255, 255, 255, 255}, std::size_t variant = 0)
        {
            sl::Instance3D instance;
            instance.position = {x, 0.0f, z};
            instance.yaw = yaw;
            instance.scale = scale;
            instance.tint = tint;
            placed[variant % placed.size()].push_back(instance);
        }
        void bake()
        {
            for (std::size_t i = 0; i < variants.size(); ++i)
                meshes.push_back(sl::bake_instances(variants[i], placed[i]));
        }
        void draw(const sl::Material &material) const
        {
            for (const sl::Mesh *mesh : meshes)
                if (mesh)
                    sl::draw_mesh(mesh, glm::mat4(1.0f), material);
        }
        void destroy()
        {
            for (sl::Mesh *mesh : meshes)
                sl::destroy_mesh(mesh);
        }
    };

    /** Direction a part's +Z front must face to look along (dx, dz). */
    float facing(float dx, float dz) { return std::atan2(dx, dz); }
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
    sl::set_window_title("simlib - Town and airfield (extown)");
    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 4);
    sl::ShadowMap3D *shadow_map = sl::create_shadow_map_3d(4096);
    sl::Mesh *ground = sl::create_plane_mesh(1500.0f, 1500.0f, {116, 132, 96, 255});
    sl::Mesh *slab = sl::create_cube_mesh(1.0f);
    if (!surface || !shadow_map || !ground || !slab)
    {
        std::fprintf(stderr, "3D unavailable: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    Kind roads({sl::make_road_data(10.0f, 6.0f)}), runway({sl::make_road_data(10.0f, 14.0f)});
    Kind houses({sl::make_house_data(6, 5, 3, {214, 196, 160, 255}, {150, 64, 48, 255}, 1), sl::make_house_data(6, 5, 3, {190, 200, 210, 255}, {70, 84, 110, 255}, 2),
                 sl::make_house_data(6.5f, 5, 3.2f, {226, 210, 170, 255}, {96, 96, 100, 255}, 3)});
    Kind offices({sl::make_office_data(14, 10, 5, {170, 172, 176, 255}, 1), sl::make_office_data(12, 9, 4, {196, 180, 160, 255}, 2)});
    Kind towers({sl::make_skyscraper_data(14, 28, {66, 96, 128, 255}, 1), sl::make_skyscraper_data(14, 36, {70, 110, 100, 255}, 2)});
    Kind hangars({sl::make_hangar_data()}), fences({sl::make_fence_data()}), walls({sl::make_wall_data()});
    Kind water_towers({sl::make_water_tower_data()}), tanks({sl::make_tank_data()});
    Kind radars({sl::make_radar_data()}), domes({sl::make_domed_radar_data()}), control_towers({sl::make_control_tower_data()});
    Kind lamps({sl::make_street_lamp_data()});
    Kind barrels({sl::make_barrel_data()}), crates({sl::make_crate_data()});
    Kind containers({sl::make_container_data(6.1f, 2.44f, 2.6f, {160, 52, 42, 255}, 1), sl::make_container_data(6.1f, 2.44f, 2.6f, {50, 90, 150, 255}, 2),
                     sl::make_container_data(6.1f, 2.44f, 2.6f, {220, 170, 50, 255}, 3)});
    Kind cars({sl::make_car_data(4.4f, {190, 44, 40, 255}, 1), sl::make_car_data(4.4f, {50, 90, 170, 255}, 2), sl::make_car_data(4.4f, {220, 200, 70, 255}, 3),
               sl::make_car_data(4.4f, {236, 236, 236, 255}, 4)});
    Kind planes({sl::make_aeroplane_data()}), ships({sl::make_spaceship_data(9.0f)});
    Kind trees({sl::make_tree_data(6, 2, {104, 74, 48, 255}, {64, 134, 58, 255}, 1), sl::make_tree_data(7, 2.3f, {104, 74, 48, 255}, {88, 140, 52, 255}, 2),
                sl::make_tree_data(5, 1.8f, {104, 74, 48, 255}, {52, 116, 70, 255}, 3), sl::make_conifer_data(8, 2)});
    Kind bushes({sl::make_bush_data(0.8f, {58, 118, 52, 255}, 1), sl::make_bush_data(1.0f, {70, 128, 60, 255}, 2)});
    Kind rocks({sl::make_rock_cluster_data(1.5f, {120, 118, 112, 255}, 1), sl::make_rock_cluster_data(2.0f, {128, 122, 112, 255}, 2), sl::make_rock_data(1.2f)});

    struct Slab
    {
        glm::vec3 centre;
        glm::vec2 size;
        sl::Colour colour;
        float height;
    };
    std::vector<Slab> slabs;
    std::vector<glm::vec3> lamp_lights;

    // Town: a main road along Z crossing a street along X, with a junction patch.
    for (int i = 0; i < 12; ++i)
        roads.place(0.0f, -55.0f + 10.0f * static_cast<float>(i), 0.0f);
    for (int i = 0; i < 6; ++i)
        for (const float side : {-1.0f, 1.0f})
            roads.place(side * (8.0f + 10.0f * static_cast<float>(i)), 0.0f, pi * 0.5f);
    slabs.push_back({{0.0f, 0.1f, 0.0f}, {6.0f, 6.0f}, {62, 63, 68, 255}, 0.2f});

    // Street lamps along the main road, arms reaching over the carriageway.
    for (const float z : {-48.0f, -30.0f, -12.0f, 12.0f, 30.0f, 48.0f})
    {
        for (const float side : {-1.0f, 1.0f})
        {
            const float yaw = facing(-side, 0.0f);
            lamps.place(side * 3.7f, z, yaw);
            lamp_lights.push_back({side * 3.7f - side * 0.9f, 5.9f, z});
        }
    }

    // Houses and their fences and trees along the north half of the main road.
    std::uint32_t seed = 7;
    for (const float z : {-48.0f, -36.0f, -24.0f, -12.0f})
    {
        for (const float side : {-1.0f, 1.0f})
        {
            const std::size_t variant = static_cast<std::size_t>(hash01(++seed) * 3.0f);
            houses.place(side * 15.0f, z, facing(-side, 0.0f), 1.0f, {255, 255, 255, 255}, variant);
            fences.place(side * 9.0f, z, pi * 0.5f);
            bushes.place(side * 11.0f, z - 2.2f, 0.0f, 0.9f, {255, 255, 255, 255}, variant);
            trees.place(side * 24.0f, z + 3.0f, 0.0f, 0.8f + 0.5f * hash01(++seed), {255, 255, 255, 255}, variant);
        }
    }
    for (int i = 0; i < 11; ++i)
        walls.place(-30.0f + 6.0f * static_cast<float>(i), -62.0f, 0.0f);

    // Downtown to the south.
    offices.place(-15.0f, 17.0f, pi * 0.5f);
    offices.place(15.0f, 17.0f, -pi * 0.5f, 1.0f, {255, 255, 255, 255}, 1);
    towers.place(-17.0f, 36.0f, pi * 0.5f);
    towers.place(17.0f, 44.0f, -pi * 0.5f, 1.0f, {255, 255, 255, 255}, 1);
    offices.place(-15.0f, 58.0f, pi * 0.5f, 1.0f, {255, 255, 255, 255}, 1);

    // Traffic.
    cars.place(-1.5f, -40.0f, pi, 1.0f, {255, 255, 255, 255}, 0);
    cars.place(1.5f, -25.0f, 0.0f, 1.0f, {255, 255, 255, 255}, 1);
    cars.place(-1.5f, 8.0f, pi, 1.0f, {255, 255, 255, 255}, 2);
    cars.place(1.5f, 27.0f, 0.0f, 1.0f, {255, 255, 255, 255}, 3);
    cars.place(-23.0f, -1.5f, pi * 0.5f, 1.0f, {255, 255, 255, 255}, 1);
    cars.place(30.0f, 1.5f, -pi * 0.5f, 1.0f, {255, 255, 255, 255}, 0);

    // Outskirts: a water tower, a tank farm and a work yard.
    water_towers.place(-40.0f, -30.0f, 0.0f);
    tanks.place(-44.0f, 34.0f, 0.0f);
    tanks.place(-52.0f, 42.0f, 0.0f, 0.8f);
    tanks.place(-38.0f, 46.0f, 0.0f, 0.9f);
    for (int i = 0; i < 6; ++i)
        barrels.place(-30.0f + 0.9f * static_cast<float>(i % 3), 70.0f + 0.9f * static_cast<float>(i / 3), 0.0f);
    for (int i = 0; i < 5; ++i)
        crates.place(-24.0f + 1.1f * static_cast<float>(i % 3), 70.0f + 1.1f * static_cast<float>(i / 3), 0.3f * static_cast<float>(i));
    containers.place(-18.0f, 76.0f, 0.0f, 1.0f, {255, 255, 255, 255}, 0);
    containers.place(-18.0f, 79.0f, 0.0f, 1.0f, {255, 255, 255, 255}, 1);
    containers.place(-11.0f, 76.0f, 0.1f, 1.0f, {255, 255, 255, 255}, 2);

    // Airfield: an apron, a runway along X and the buildings facing it.
    slabs.push_back({{115.0f, 0.025f, -23.0f}, {112.0f, 52.0f}, {150, 150, 148, 255}, 0.05f});
    slabs.push_back({{160.0f, 0.06f, -10.0f}, {16.0f, 16.0f}, {112, 114, 120, 255}, 0.12f});
    for (int i = 0; i < 10; ++i)
        runway.place(66.0f + 10.0f * static_cast<float>(i), 10.0f, pi * 0.5f);
    hangars.place(82.0f, -30.0f, 0.0f);
    control_towers.place(118.0f, -38.0f, 0.0f);
    radars.place(142.0f, -42.0f, pi * 0.8f);
    domes.place(160.0f, -38.0f, 0.0f);
    planes.place(116.0f, -10.0f, pi * 0.5f);
    planes.place(150.0f, 10.0f, -pi * 0.5f);
    ships.place(160.0f, -10.0f, pi * 0.9f);
    for (int i = 0; i < 4; ++i)
        containers.place(62.0f, -48.0f + 3.0f * static_cast<float>(i), pi * 0.5f, 1.0f, {255, 255, 255, 255}, static_cast<std::size_t>(i));
    for (int i = 0; i < 3; ++i)
        barrels.place(100.0f + 0.9f * static_cast<float>(i), -18.0f, 0.0f);
    crates.place(100.0f, -20.5f, 0.2f);
    crates.place(101.2f, -20.5f, -0.1f);
    for (int i = 0; i < 17; ++i)
        fences.place(63.0f + 6.0f * static_cast<float>(i), -52.0f, pi);
    for (int i = 0; i < 4; ++i)
        lamps.place(70.0f + 28.0f * static_cast<float>(i), 3.5f, pi);
    for (int i = 0; i < 4; ++i)
        lamp_lights.push_back({70.0f + 28.0f * static_cast<float>(i), 5.9f, 3.5f - 0.9f});
    cars.place(75.0f, -8.0f, 0.4f, 1.0f, {255, 255, 255, 255}, 3);

    // Trees, bushes and rocks scattered where nothing else is.
    const auto occupied = [](float x, float z)
    {
        const bool town = std::abs(x) < 32.0f && z > -66.0f && z < 88.0f;
        const bool field = x > 52.0f && x < 180.0f && z > -62.0f && z < 24.0f;
        const bool works = std::hypot(x + 45.0f, z - 40.0f) < 14.0f || std::hypot(x + 40.0f, z + 30.0f) < 8.0f;
        return town || field || works;
    };
    int scattered = 0;
    for (std::uint32_t i = 0; i < 700 && scattered < 150; ++i)
    {
        const float x = -90.0f + 290.0f * hash01(i * 3u + 1000u), z = -95.0f + 190.0f * hash01(i * 3u + 1001u);
        if (occupied(x, z))
            continue;
        const float pick = hash01(i * 3u + 1002u), yaw = 2.0f * pi * hash01(i + 5000u);
        const std::size_t variant = static_cast<std::size_t>(hash01(i + 9000u) * 4.0f);
        if (pick < 0.62f)
            trees.place(x, z, yaw, 0.75f + 0.6f * hash01(i + 7000u), {255, 255, 255, 255}, variant);
        else if (pick < 0.84f)
            bushes.place(x, z, yaw, 0.8f + 0.5f * hash01(i + 7000u), {255, 255, 255, 255}, variant);
        else
            rocks.place(x, z, yaw, 0.8f + 0.6f * hash01(i + 7000u), {255, 255, 255, 255}, variant);
        ++scattered;
    }

    Kind *all[] = {&roads,  &runway,         &houses, &offices, &towers, &hangars, &fences, &walls,   &water_towers, &tanks, &radars,
                   &domes,  &control_towers, &lamps,  &barrels, &crates, &containers, &cars,  &planes,  &ships,         &trees, &bushes, &rocks};
    std::size_t triangles = 0;
    for (Kind *kind : *&all)
    {
        kind->bake();
        for (std::size_t i = 0; i < kind->variants.size(); ++i)
            triangles += kind->variants[i].indices.size() / 3 * kind->placed[i].size();
    }

    struct View
    {
        glm::vec3 target;
        float distance, yaw;
    };
    const View views[2] = {{{0.0f, 6.0f, 10.0f}, 125.0f, 0.5f}, {{118.0f, 5.0f, -14.0f}, 70.0f, 0.2f}};
    int view = 0;
    bool night = false, paused = false, running = true;
    float yaw = views[0].yaw, pitch = 0.5f, zoom = 1.0f;
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
                if (key == sl::Event::Key::escape)
                    running = false;
                else if (key == sl::Event::Key::digit_1 || key == sl::Event::Key::digit_2)
                {
                    view = key == sl::Event::Key::digit_1 ? 0 : 1;
                    yaw = views[view].yaw;
                }
                else if (key == sl::Event::Key::letter_n)
                    night = !night;
                else if (key == sl::Event::Key::space)
                    paused = !paused;
            }
            sl::display_handle_event(event);
        }
        if (sl::key_down(sl::Key::left_bracket))
            zoom = std::max(0.3f, zoom - 0.6f * frame);
        if (sl::key_down(sl::Key::right_bracket))
            zoom = std::min(2.0f, zoom + 0.6f * frame);
        const int mx = sl::mouse_x(), my = sl::mouse_y();
        if (sl::mouse_button_down(sl::MouseButton::right))
        {
            yaw -= static_cast<float>(mx - last_x) * 0.008f;
            pitch = std::clamp(pitch + static_cast<float>(my - last_y) * 0.008f, 0.08f, 1.45f);
        }
        else if (!paused)
            yaw += 0.06f * frame;
        last_x = mx;
        last_y = my;

        sl::Camera camera;
        camera.target = views[view].target;
        camera.position = camera.target + views[view].distance * zoom * glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
        camera.near_plane = 0.5f;
        camera.far_plane = 1500.0f;

        sl::Light3D sun;
        sun.direction = glm::normalize(night ? glm::vec3(-0.3f, -1.0f, -0.5f) : glm::vec3(-0.5f, -0.75f, -0.4f));
        sun.colour = night ? glm::vec3(0.3f, 0.38f, 0.65f) : glm::vec3(1.0f, 0.95f, 0.85f);
        sun.ambient = night ? 0.1f : 0.34f;
        sl::set_light_3d(sun);

        // At night the street lamps nearest the camera target light up.
        std::vector<sl::LocalLight3D> lights;
        if (night)
        {
            std::vector<glm::vec3> near_lamps = lamp_lights;
            const glm::vec3 centre = views[view].target;
            std::sort(near_lamps.begin(), near_lamps.end(), [&](const glm::vec3 &a, const glm::vec3 &b)
                      { return glm::distance(a, centre) < glm::distance(b, centre); });
            for (std::size_t i = 0; i < near_lamps.size() && i < static_cast<std::size_t>(sl::max_local_lights_3d); ++i)
            {
                sl::LocalLight3D light;
                light.position = near_lamps[i];
                light.colour = {1.0f, 0.82f, 0.55f};
                light.intensity = 90.0f;
                light.range = 24.0f;
                lights.push_back(light);
            }
        }
        sl::set_local_lights_3d(lights);

        sl::Material material;
        material.pbr = false;
        const auto draw_scene = [&]()
        {
            sl::draw_mesh(ground, glm::mat4(1.0f), material);
            sl::Material slab_material = material;
            for (const Slab &s : slabs)
            {
                slab_material.tint = s.colour;
                sl::draw_mesh(slab, glm::translate(glm::mat4(1.0f), s.centre) * glm::scale(glm::mat4(1.0f), glm::vec3(s.size.x, s.height, s.size.y)), slab_material);
            }
            for (const Kind *kind : all)
                kind->draw(material);
        };

        sl::set_shadow_map_3d(nullptr);
        if (sl::begin_shadow_pass_3d(shadow_map, views[view].target, 80.0f * zoom))
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
        sl::gprintf(16, 12, {255, 255, 255}, "extown - %s   %zu triangles   %s", view == 0 ? "town" : "airfield", triangles, night ? "night" : "day");
        sl::gprintf_center(screen_h - 22, {235, 240, 250}, "[1] Town  [2] Airfield  [N] Day/night  [Space] Pause  [R-drag] Look  [ [ ] ] Zoom");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }
    for (Kind *kind : all)
        kind->destroy();
    sl::destroy_mesh(slab);
    sl::destroy_mesh(ground);
    sl::destroy_shadow_map_3d(shadow_map);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
