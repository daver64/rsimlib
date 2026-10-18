#include "sl.h"
#include "flight.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Spaceflight with FlightWorld: a spacecraft in low Earth orbit with a main engine, retro engines and a
// twelve-thruster RCS built into thruster groups. Earth is a gravity source; the Moon follows a Kepler orbit on rails.
// Rendering uses kilometres with the ship at the origin (a floating origin), so precision holds at any distance.
//
//   Z/X: full / cut main engine    Shift/Ctrl: raise / lower throttle    B: retro engines
//   W/S: pitch   A/D: yaw   Q/E: roll   Arrow keys: RCS translation X/Y   T: rotation damping (SAS)
//   O: toggle orbit line (shown when zoomed out)
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

    // Project a point (kilometres, ship-relative world frame) to the screen; false if behind the camera.
    bool project(const glm::mat4 &view_proj, const glm::vec3 &p, int w, int h, float &x, float &y)
    {
        const glm::vec4 clip = view_proj * glm::vec4(p, 1.0f);
        if (clip.w <= 0.0f)
            return false;
        x = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(w);
        y = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * static_cast<float>(h);
        return true;
    }

    glm::vec3 to_km(const sl::dvec3 &v) { return glm::vec3(v / km); }
}

int main(int argc, char **argv)
{
    if (!sl::configure_graphics_backend_from_args(argc, argv))
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
    sl::set_window_title("simlib - Orbital Flight (exorbit)");

    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 1);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    sl::Mesh *sphere = sl::create_sphere_mesh(1.0f, 64, 32, {255, 255, 255, 255});
    sl::Mesh *cube = sl::create_cube_mesh(1.0f, {255, 255, 255, 255});

    Scenario scenario;
    build(scenario);
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
                }
                else if (key == sl::Event::Key::letter_t)
                    sas = !sas;
                else if (key == sl::Event::Key::letter_o)
                    show_orbit = !show_orbit;
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

        // Ship at the origin in kilometres; the camera orbits it in the ship's frame.
        const glm::mat3 ship_rot = glm::mat3(glm::mat3_cast(glm::quat(ship.orientation)));
        const glm::vec3 offset = static_cast<float>(cam_distance / km) *
                                 glm::vec3(std::cos(cam_pitch) * std::sin(cam_yaw), std::sin(cam_pitch),
                                           std::cos(cam_pitch) * std::cos(cam_yaw));
        sl::Camera camera;
        camera.position = ship_rot * offset;
        camera.target = glm::vec3(0.0f);
        camera.up = ship_rot * glm::vec3(0.0f, 1.0f, 0.0f);
        camera.field_of_view = 50.0f;
        camera.near_plane = std::max(1.0e-4f, glm::length(camera.position) * 0.05f);
        camera.far_plane = 1.0e6f;

        const sl::FlightBody &earth = *world.body(scenario.earth);
        const sl::FlightBody &moon = *world.body(scenario.moon);
        const glm::vec3 earth_p = to_km(earth.position - ship.position);
        const glm::vec3 moon_p = to_km(moon.position - ship.position);

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({2, 3, 8, 255});
            sl::Light3D light;
            light.direction = glm::normalize(glm::vec3(-1.0f, -0.2f, -0.4f));
            light.ambient = 0.12f;
            sl::set_light_3d(light);

            sl::Material planet;
            planet.tint = {70, 110, 190, 255};
            sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), earth_p) * glm::scale(glm::mat4(1.0f), glm::vec3(earth_radius / km)), planet);
            planet.tint = {170, 170, 165, 255};
            sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), moon_p) * glm::scale(glm::mat4(1.0f), glm::vec3(moon_radius / km)), planet);

            // The hull is drawn at real size: 4 m wide, 3 m tall and 9 m long, with a nozzle block at the stern.
            const glm::mat4 hull = glm::mat4(ship_rot);
            sl::Material metal;
            metal.tint = {210, 210, 220, 255};
            sl::draw_mesh(cube, hull * glm::scale(glm::mat4(1.0f), glm::vec3(0.004f, 0.003f, 0.009f)), metal);
            metal.tint = {230, 120, 60, 255};
            sl::draw_mesh(cube, hull * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0055f)) *
                                    glm::scale(glm::mat4(1.0f), glm::vec3(0.0015f, 0.0015f, 0.0015f)),
                          metal);
            metal.tint = {90, 160, 255, 255};
            sl::draw_mesh(cube, hull * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.0050f)) *
                                    glm::scale(glm::mat4(1.0f), glm::vec3(0.0025f, 0.002f, 0.0015f)),
                          metal);
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
            const glm::vec3 eye = camera.position;
            const float earth_r = static_cast<float>(earth_radius / km);
            bool have_previous = false;
            float px = 0.0f, py = 0.0f;
            for (const sl::dvec3 &point : path)
            {
                const glm::vec3 p = to_km(point + earth.position - ship.position);
                // Hide the part of the orbit that Earth covers from the camera.
                const glm::vec3 ray = p - eye;
                const float ray_length = glm::length(ray);
                const glm::vec3 dir = ray / ray_length;
                const glm::vec3 to_centre = earth_p - eye;
                const float along = glm::dot(to_centre, dir);
                const bool hidden = along > 0.0f && along < ray_length &&
                                    glm::length(to_centre - dir * along) < earth_r * 0.999f;
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
                           "[Z/X] Main  [Shift/Ctrl] Throttle  [B] Retro  [WASDQE] Rotate  [Arrows] Translate  [T] SAS  [O] Orbit  [,/.] Warp  [R-drag] Look  [ [ ] ] Zoom");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::destroy_mesh(sphere);
    sl::destroy_mesh(cube);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
