#include "sl.h"
#include "flight.h"
#include "example_fx.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

// Spaceflight with FlightWorld: a spacecraft in low Earth orbit with a main engine, retro engines and a
// twelve-thruster RCS built into thruster groups. Earth is a gravity source; the Moon follows a Kepler orbit on rails.
// Earth is a procedural streaming planet (sl::Terrain on a sphere) with an atmosphere and a cloud shell, the Moon is a
// textured sphere, and the ship is an Apollo-style command and service module whose thrusters emit glowing exhaust
// particles and drive synthesised rocket and RCS sound loops. Rendering uses metres with the camera at the origin and
// everything drawn relative to the eye (a floating origin), so precision holds at any distance.
//
//   Z/X: full / cut main engine    Shift/Ctrl: raise / lower throttle    B: retro engines
//   W/S: pitch   A/D: yaw   Q/E: roll   Arrow keys: RCS translation X/Y   T: rotation damping (SAS)
//   O: toggle orbit line (shown when zoomed out)   M: sound
//   , / . : time warp (slows to 1x while thrusting)   Right-drag: look around   [ / ]: zoom   R: reset   Esc: exit

namespace
{
    constexpr double earth_gm = 3.986004418e14;
    constexpr double earth_radius = 6.371e6;
    constexpr double moon_gm = 4.9028e12;
    constexpr double moon_radius = 1.7374e6;
    constexpr double km = 1000.0;
    const double warp_levels[] = {1, 2, 5, 10, 100, 1000, 10000, 100000};
    constexpr int warp_count = sizeof(warp_levels) / sizeof(warp_levels[0]);

    struct Scenario
    {
        sl::FlightWorld world;
        int earth = 0;
        int moon = 0;
        int ship = 0;
    };

    void add_rcs(sl::FlightBody &b, sl::dvec3 position, sl::dvec3 direction)
    {
        sl::Thruster t;
        t.position = position;
        t.direction = direction;
        t.max_thrust = 400.0;
        t.isp = 250.0;
        t.tank = 1;
        b.thrusters.push_back(t);
    }

    void build(Scenario &s)
    {
        s = Scenario();
        sl::FlightWorld &w = s.world;
        w.integrator = sl::FlightIntegrator::rk4;
        w.max_substep = 1.0;
        w.max_substeps_per_step = 2000;

        s.earth = w.create_body("earth");
        sl::FlightBody *earth = w.body(s.earth);
        earth->mode = sl::FlightBodyMode::kinematic;
        earth->gm = earth_gm;
        earth->radius = earth_radius;

        s.moon = w.create_body("moon");
        sl::FlightBody *moon = w.body(s.moon);
        moon->gm = moon_gm;
        moon->radius = moon_radius;
        sl::OrbitElements lunar;
        lunar.semi_major_axis = 3.844e8;
        lunar.eccentricity = 0.0549;
        lunar.inclination = 0.09;
        lunar.mean_anomaly = 1.0;
        const sl::OrbitState lunar_state = sl::state_from_orbit(earth_gm, lunar);
        moon->position = lunar_state.position;
        moon->velocity = lunar_state.velocity;
        w.set_rails(s.moon, s.earth);

        s.ship = w.create_body("ship");
        sl::FlightBody *ship = w.body(s.ship);
        ship->dry_mass = 5000.0;
        ship->dry_inertia = {9000.0, 9000.0, 4000.0};
        ship->tanks.push_back({3000.0, 3000.0}); // main propellant
        ship->tanks.push_back({100.0, 100.0});   // RCS propellant
        const double r = earth_radius + 400.0e3;
        ship->position = {r, 0.0, 0.0};
        ship->velocity = {0.0, 0.0, -std::sqrt(earth_gm / r)};
        // Nose along the direction of travel, roof pointing away from Earth.
        ship->orientation = glm::angleAxis(-1.5707963267948966, sl::dvec3(0.0, 0.0, 1.0));

        sl::Thruster main;
        main.direction = {0, 0, -1};
        main.position = {0, 0, 4};
        main.max_thrust = 20000.0;
        main.isp = 320.0;
        main.tank = 0;
        ship->thrusters.push_back(main);
        sl::Thruster retro = main;
        retro.direction = {0, 0, 1};
        retro.position = {0, 0, -4};
        retro.max_thrust = 5000.0;
        ship->thrusters.push_back(retro);

        for (double z : {-3.0, 3.0})
            for (const sl::dvec3 d : {sl::dvec3(1, 0, 0), sl::dvec3(-1, 0, 0), sl::dvec3(0, 1, 0), sl::dvec3(0, -1, 0)})
                add_rcs(*ship, {0, 0, z}, d);
        for (double x : {-1.5, 1.5})
            for (double y : {1.0, -1.0})
                add_rcs(*ship, {x, 0, 0}, {0, y, 0});
        w.build_standard_groups(s.ship);
    }

    struct Exhaust
    {
        glm::vec3 position, velocity;
        float age, life, size;
    };

    std::vector<sl::TerrainLayer> planet_layers()
    {
        std::vector<sl::TerrainLayer> layers(4);
        layers[0].colour = {18, 62, 135, 255}; // ocean
        layers[0].max_height = 1.0;
        layers[0].height_blend = 2.0;
        layers[1].colour = {58, 112, 52, 255}; // lowland
        layers[1].min_height = 1.0;
        layers[1].max_height = 2600.0;
        layers[1].height_blend = 600.0;
        layers[2].colour = {125, 112, 98, 255}; // highland rock
        layers[2].min_height = 2200.0;
        layers[2].max_height = 4600.0;
        layers[2].height_blend = 500.0;
        layers[3].colour = {245, 246, 250, 255}; // snow
        layers[3].min_height = 4300.0;
        layers[3].height_blend = 400.0;
        return layers;
    }

    // Chunks stream on worker threads; waiting once at the start avoids a coarse first frame.
    void settle(sl::Terrain &terrain, const sl::TerrainView &view)
    {
        for (int attempt = 0; attempt < 400; ++attempt)
        {
            terrain.update(view);
            if (terrain.stats().pending_chunks == 0)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    struct EngineSound
    {
        sl::Sample *rocket = nullptr, *hiss = nullptr;
        std::uint64_t rocket_voice = 0, hiss_voice = 0;

        void start()
        {
            rocket = fx::make_rocket_sound();
            hiss = fx::make_hiss_sound();
            rocket_voice = rocket ? sl::play_sample(rocket, 0, 128, 1000, -1) : 0;
            hiss_voice = hiss ? sl::play_sample(hiss, 0, 128, 1000, -1) : 0;
        }

        void set(double rocket_level, double rcs_level, bool on)
        {
            const double gain = on ? 1.0 : 0.0;
            if (rocket_voice)
                sl::set_voice_volume(rocket_voice, static_cast<int>(std::clamp(rocket_level * gain, 0.0, 1.0) * 255.0));
            if (hiss_voice)
                sl::set_voice_volume(hiss_voice, static_cast<int>(std::clamp(rcs_level * 0.6 * gain, 0.0, 1.0) * 255.0));
        }

        void stop()
        {
            sl::stop_all_samples();
            for (sl::Sample **s : {&rocket, &hiss})
            {
                if (*s)
                    sl::destroy_sample(*s);
                *s = nullptr;
            }
            rocket_voice = hiss_voice = 0;
            sl::audio_fx_shutdown();
        }
    };

    // Project a point (ship-relative world frame, metres) to the screen; false if behind the camera.
    bool project(const glm::mat4 &view_proj, const glm::vec3 &p, int w, int h, float &x, float &y)
    {
        const glm::vec4 clip = view_proj * glm::vec4(p, 1.0f);
        if (clip.w <= 0.0f)
            return false;
        x = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(w);
        y = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * static_cast<float>(h);
        return true;
    }

}

int main(int argc, char **argv)
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv))
    {
        return -1;
    }
    constexpr int screen_w = 1100;
    constexpr int screen_h = 680;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
    {
        std::fprintf(stderr, "Failed to initialize display: %s\n", sl::last_error().c_str());
        return -1;
    }
    sl::set_window_title("simlib - Orbital Flight (exorbit)");

    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 1);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    sl::Mesh *glow_mesh = sl::create_sphere_mesh(1.0f, 12, 8, {255, 255, 255, 255});
    sl::Mesh *moon_mesh = sl::create_sphere_mesh(static_cast<float>(moon_radius), 64, 32, {255, 255, 255, 255});
    sl::Mesh *cloud_shell = sl::create_sphere_mesh(static_cast<float>(earth_radius + 60.0e3), 128, 64, {255, 255, 255, 255});
    sl::Bitmap *cloud_texture = fx::make_cloud_texture();
    sl::Bitmap *moon_texture = fx::make_moon_texture();
    fx::Capsule capsule;
    if (!glow_mesh || !moon_mesh || !cloud_shell || !cloud_texture || !moon_texture || !capsule.create())
    {
        std::fprintf(stderr, "Failed to create meshes: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    sl::NoiseParameters noise;
    noise.seed = 11;
    noise.frequency = 1.0 / 3.0e6;
    noise.amplitude = 6000.0;
    noise.base_height = -1200.0;
    noise.floor_height = 0.0;
    sl::TerrainConfig config;
    config.shape = sl::TerrainShape::sphere;
    config.radius = earth_radius;
    config.finest_cell_size = 4.0;
    config.layers = planet_layers();
    std::unique_ptr<sl::Terrain> terrain;
    try
    {
        terrain = std::make_unique<sl::Terrain>(config, std::make_shared<sl::NoiseSource>(noise));
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "Terrain unavailable: %s\n", e.what());
        sl::shutdown();
        return -1;
    }

    Scenario scenario;
    build(scenario);
    std::vector<Exhaust> exhaust;
    EngineSound engine;
    engine.start();
    bool sound_on = true;
    bool first_frame = true;
    double clock = 0.0;
    std::uint32_t rng = 7u;
    const auto random01 = [&rng]
    {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>(rng >> 8) / 16777216.0f - 0.5f;
    };
    std::vector<float> spawn_debt(64, 0.0f);
    float cam_yaw = 0.6f, cam_pitch = 0.3f;
    double cam_distance = 40.0; // metres
    int warp = 0;
    bool sas = false;
    bool show_orbit = true;
    bool running = true;
    int last_mouse_x = 0, last_mouse_y = 0;
    std::uint32_t last_time = sl::time_ms();

    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const double frame = std::clamp(static_cast<double>(now - last_time) * 0.001, 0.001, 0.1);
        last_time = now;
        clock += frame;
        sl::FlightWorld &world = scenario.world;

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
                    build(scenario);
                    warp = 0;
                    exhaust.clear();
                }
                else if (key == sl::Event::Key::letter_t)
                    sas = !sas;
                else if (key == sl::Event::Key::letter_o)
                    show_orbit = !show_orbit;
                else if (key == sl::Event::Key::letter_m)
                    sound_on = !sound_on;
                else if (key == sl::Event::Key::letter_z)
                    world.set_group_level(scenario.ship, "main", 1.0);
                else if (key == sl::Event::Key::letter_x)
                    world.set_group_level(scenario.ship, "main", 0.0);
                else if (key == sl::Event::Key::period)
                    warp = std::min(warp + 1, warp_count - 1);
                else if (key == sl::Event::Key::comma)
                    warp = std::max(warp - 1, 0);
            }
            sl::display_handle_event(event);
        }

        sl::FlightBody &ship = *world.body(scenario.ship);

        // Throttle, retro and RCS controls.
        const double throttle_rate = 0.5 * frame;
        if (sl::key_down(sl::Key::left_shift))
            world.set_group_level(scenario.ship, "main", std::min(1.0, ship.thrusters[0].level + throttle_rate));
        if (sl::key_down(sl::Key::left_control))
            world.set_group_level(scenario.ship, "main", std::max(0.0, ship.thrusters[0].level - throttle_rate));
        world.set_group_level(scenario.ship, "retro", sl::key_down(sl::Key::letter_b) ? 1.0 : 0.0);

        auto axis = [](sl::Key plus, sl::Key minus)
        { return (sl::key_down(plus) ? 1.0 : 0.0) - (sl::key_down(minus) ? 1.0 : 0.0); };
        sl::dvec3 rotation(axis(sl::Key::letter_s, sl::Key::letter_w), axis(sl::Key::letter_a, sl::Key::letter_d),
                           axis(sl::Key::letter_e, sl::Key::letter_q));
        if (sas && glm::length(rotation) == 0.0)
            rotation = glm::clamp(-ship.angular_velocity * 30.0, sl::dvec3(-1.0), sl::dvec3(1.0));
        world.set_rcs_rotation(scenario.ship, rotation.x, rotation.y, rotation.z);
        world.set_rcs_translation(scenario.ship, axis(sl::Key::arrow_right, sl::Key::arrow_left),
                                  axis(sl::Key::arrow_up, sl::Key::arrow_down), 0.0);

        const bool thrusting = glm::length(rotation) > 0.0 || ship.thrusters[0].level > 0.0 || ship.thrusters[1].level > 0.0 ||
                               glm::length(ship.rcs_translation) > 0.0;
        const double warp_factor = thrusting ? 1.0 : warp_levels[warp];
        world.step(frame * warp_factor);

        // Camera control.
        const int mx = sl::mouse_x(), my = sl::mouse_y();
        if (sl::mouse_button_down(sl::MouseButton::right))
        {
            cam_yaw -= static_cast<float>(mx - last_mouse_x) * 0.008f;
            cam_pitch = std::clamp(cam_pitch + static_cast<float>(my - last_mouse_y) * 0.008f, -1.5f, 1.5f);
        }
        last_mouse_x = mx;
        last_mouse_y = my;
        if (sl::key_down(sl::Key::left_bracket))
            cam_distance = std::max(15.0, cam_distance / (1.0 + 2.0 * frame));
        if (sl::key_down(sl::Key::right_bracket))
            cam_distance = std::min(3.0e7, cam_distance * (1.0 + 2.0 * frame));

        // The camera orbits the ship in its frame. It sits at the origin and everything is drawn relative to the eye.
        const glm::mat3 ship_rot = glm::mat3(glm::mat3_cast(glm::quat(ship.orientation)));
        const glm::vec3 offset = static_cast<float>(cam_distance) *
                                 glm::vec3(std::cos(cam_pitch) * std::sin(cam_yaw), std::sin(cam_pitch),
                                           std::cos(cam_pitch) * std::cos(cam_yaw));
        const sl::dvec3 offset_world = sl::dvec3(ship_rot * offset);
        const sl::dvec3 eye_world = ship.position + offset_world;
        const glm::vec3 ship_rel = glm::vec3(-offset_world);
        const double eye_distance = glm::length(eye_world);

        const sl::FlightBody &earth = *world.body(scenario.earth);
        const sl::FlightBody &moon = *world.body(scenario.moon);
        const glm::vec3 earth_p = glm::vec3(earth.position - eye_world);
        const glm::vec3 moon_p = glm::vec3(moon.position - eye_world);

        sl::TerrainView view;
        view.position = eye_world;
        view.forward = glm::normalize(sl::dvec3(ship_rel));
        view.up = glm::normalize(sl::dvec3(ship_rot * glm::vec3(0.0f, 1.0f, 0.0f)));
        view.fov_y_degrees = 50.0;
        view.aspect = static_cast<double>(screen_w) / screen_h;
        view.viewport_height = screen_h;
        view.far_distance = eye_distance + earth_radius * 1.3;
        if (first_frame)
            settle(*terrain, view);
        else
            terrain->update(view);
        first_frame = false;

        // Exhaust particles live in the ship's frame; each firing thruster feeds them in proportion to its thrust.
        const float fdt = static_cast<float>(frame);
        for (Exhaust &p : exhaust)
        {
            p.age += fdt;
            p.position += p.velocity * fdt;
        }
        exhaust.erase(std::remove_if(exhaust.begin(), exhaust.end(), [](const Exhaust &p) { return p.age >= p.life; }),
                      exhaust.end());
        double rocket_level = 0.0, rcs_level = 0.0;
        for (std::size_t i = 0; i < ship.thrusters.size(); ++i)
        {
            const sl::Thruster &t = ship.thrusters[i];
            const bool engine_thruster = t.max_thrust > 1000.0;
            if (engine_thruster)
                rocket_level = std::max(rocket_level, t.level);
            else if (t.level > 0.0)
                rcs_level = 1.0;
            if (t.level <= 0.01 || i >= spawn_debt.size())
                continue;
            const float rate = engine_thruster ? static_cast<float>(220.0 * t.level * std::sqrt(t.max_thrust / 20000.0)) : 45.0f;
            spawn_debt[i] += fdt * rate;
            const glm::vec3 dir = glm::vec3(t.direction);
            for (; spawn_debt[i] >= 1.0f; spawn_debt[i] -= 1.0f)
            {
                Exhaust p;
                const float jitter = engine_thruster ? 0.12f : 0.25f;
                const glm::vec3 spread(random01() * jitter, random01() * jitter, random01() * jitter);
                const float clearance = engine_thruster ? (t.direction.z < 0.0 ? 1.7f : 1.0f) : 2.0f;
                p.position = glm::vec3(t.position) - dir * clearance + spread * 0.5f;
                p.velocity = (-dir + spread) * (engine_thruster ? 16.0f : 7.0f);
                p.age = 0.0f;
                p.life = (engine_thruster ? 0.45f : 0.3f) + 0.35f * (random01() + 0.5f);
                p.size = engine_thruster ? 0.55f + 0.3f * (random01() + 0.5f) : 0.18f + 0.1f * (random01() + 0.5f);
                exhaust.push_back(p);
            }
        }
        engine.set(rocket_level, rcs_level, sound_on);

        sl::Camera camera;
        camera.position = glm::vec3(0.0f);
        camera.target = ship_rel;
        camera.up = glm::vec3(view.up);
        camera.field_of_view = 50.0f;
        camera.near_plane = static_cast<float>(std::max(1.0, cam_distance * 0.3));
        // Reach the Moon only once zoomed out far enough that the near plane keeps depth precision.
        camera.far_plane = static_cast<float>(camera.near_plane > 1.0e3f ? 1.0e9 : view.far_distance);

        sl::Environment3D environment;
        const glm::vec3 to_sun = glm::normalize(glm::vec3(1.0f, 0.35f, 0.55f));
        environment.sun_direction = -to_sun;
        environment.up = glm::vec3(glm::normalize(eye_world));
        environment.camera_altitude = static_cast<float>(eye_distance - earth_radius);
        environment.camera_position = eye_world;
        environment.time = clock;
        environment.planet = true;
        environment.planet_radius = static_cast<float>(earth_radius);
        environment.sun_intensity = 20.0f;
        environment.haze_scale = 0.35f;
        environment.ground_albedo = {0.1f, 0.25f, 0.12f};
        environment.fog_enabled = true;
        environment.fog_density = 1.4e-4f;

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({2, 3, 8, 255});
            sl::set_environment_3d(environment);
            sl::draw_sky_3d();
            sl::Light3D light;
            light.direction = environment.sun_direction;
            light.colour = sl::atmosphere_sun_colour(environment);
            light.ambient = 0.14f;
            sl::set_light_3d(light);

            sl::Material ground;
            terrain->draw(eye_world, ground);

            // The sky's own clouds are hidden by terrain from space, so the weather is a translucent textured shell.
            sl::Material cloud;
            cloud.texture = cloud_texture;
            cloud.blend = sl::MaterialBlend::alpha;
            cloud.fog = false;
            sl::draw_mesh(cloud_shell, glm::translate(glm::mat4(1.0f), earth_p), cloud);

            sl::Material rock;
            rock.texture = moon_texture;
            rock.fog = false;
            sl::draw_mesh(moon_mesh, glm::translate(glm::mat4(1.0f), moon_p), rock);

            const glm::mat4 hull = glm::translate(glm::mat4(1.0f), ship_rel) * glm::mat4(ship_rot);
            light.ambient = 0.5f;
            sl::set_light_3d(light);
            capsule.draw(hull);

            // Exhaust: additive glowing particles, each a hot core inside a wider, fainter halo.
            sl::Material glow;
            glow.lit = false;
            glow.fog = false;
            glow.blend = sl::MaterialBlend::additive;
            for (const Exhaust &p : exhaust)
            {
                const float age = p.age / p.life;
                const float fade = (1.0f - age) * (1.0f - age);
                const sl::Colour hot = age < 0.35f ? sl::Colour{255, 235, 190, 255} : sl::Colour{255, 120, 40, 255};
                const auto at = [&](float size, sl::Colour colour, float alpha)
                {
                    glow.tint = colour;
                    glow.tint.alpha = static_cast<std::uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f));
                    sl::draw_mesh(glow_mesh, hull * glm::translate(glm::mat4(1.0f), p.position) *
                                                 glm::scale(glm::mat4(1.0f), glm::vec3(size)),
                                  glow);
                };
                const float size = p.size * (0.6f + 1.2f * age);
                at(size * 2.6f, {255, 110, 40, 255}, 0.10f * fade);
                at(size, hot, 0.55f * fade);
            }
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {2, 3, 8});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);

        // Orbit around Earth drawn as a polyline.
        sl::OrbitElements elements;
        const bool have_orbit = world.orbit_of(scenario.ship, scenario.earth, elements);
        const glm::mat4 view_proj = camera.projection_matrix(static_cast<float>(screen_w) / screen_h) * camera.view_matrix();
        // At close range the orbit is a straight line through the ship, so only draw it once zoomed out.
        if (have_orbit && show_orbit && cam_distance > 5.0e3)
        {
            const std::vector<sl::dvec3> path = sl::orbit_path(earth_gm, elements, 360, 2.0e9);
            const float earth_r = static_cast<float>(earth_radius);
            bool have_previous = false;
            float px = 0.0f, py = 0.0f;
            for (const sl::dvec3 &point : path)
            {
                const glm::vec3 p = glm::vec3(point + earth.position - eye_world);
                // Hide the part of the orbit that Earth covers from the camera.
                const float ray_length = glm::length(p);
                const glm::vec3 dir = p / ray_length;
                const float along = glm::dot(earth_p, dir);
                const bool hidden = along > 0.0f && along < ray_length &&
                                    glm::length(earth_p - dir * along) < earth_r * 0.999f;
                float x = 0.0f, y = 0.0f;
                const bool visible = !hidden && project(view_proj, p, screen_w, screen_h, x, y) &&
                                     std::fabs(x) < 1.0e4f && std::fabs(y) < 1.0e4f;
                if (visible && have_previous)
                    sl::line(sl::screen, px, py, x, y, {80, 255, 120});
                have_previous = visible;
                px = x;
                py = y;
            }
        }

        const double altitude = glm::length(ship.position - earth.position) - earth_radius;
        const double speed = glm::length(ship.velocity - earth.velocity);
        sl::gprintf(16, 12, {240, 240, 240}, "exorbit - orbital flight%s", altitude < 0.0 ? "   *** IMPACT - press R ***" : "");
        sl::gprintf(16, 34, {200, 220, 255}, "Altitude %.1f km   Speed %.3f km/s", altitude / km, speed / km);
        if (have_orbit && elements.eccentricity < 1.0)
            sl::gprintf(16, 52, {120, 255, 150}, "Apoapsis %.1f km   Periapsis %.1f km   Period %.1f min",
                        (elements.apoapsis() - earth_radius) / km, (elements.periapsis() - earth_radius) / km,
                        elements.period(earth_gm) / 60.0);
        else if (have_orbit)
            sl::gprintf(16, 52, {255, 200, 120}, "Escape trajectory   Periapsis %.1f km", (elements.periapsis() - earth_radius) / km);
        sl::gprintf(16, 70, {255, 220, 150}, "Throttle %3.0f%%   Main fuel %.0f kg   RCS fuel %.1f kg   Mass %.0f kg",
                    ship.thrusters[0].level * 100.0, ship.tanks[0].mass, ship.tanks[1].mass, ship.mass());
        sl::gprintf(16, 88, {220, 220, 220}, "Warp x%.0f%s   SAS %s   Spin %.2f %.2f %.2f deg/s", warp_factor,
                    thrusting && warp > 0 ? " (thrusting)" : "", sas ? "ON" : "off",
                    glm::degrees(ship.angular_velocity.x), glm::degrees(ship.angular_velocity.y),
                    glm::degrees(ship.angular_velocity.z));
        sl::gprintf_center(screen_h - 22, {200, 210, 220},
                           "[Z/X] Main  [Shift/Ctrl] Throttle  [B] Retro  [WASDQE] Rotate  [Arrows] Translate  [T] SAS  [O] Orbit  [,/.] Warp  [R-drag] Look  [ [ ] ] Zoom  [M] Sound");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    engine.stop();
    terrain.reset();
    capsule.destroy();
    for (sl::Mesh *m : {glow_mesh, moon_mesh, cloud_shell})
        sl::destroy_mesh(m);
    sl::destroy_bitmap(cloud_texture);
    sl::destroy_bitmap(moon_texture);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
