#include "sl.h"
#include "flight.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Aircraft flight with FlightWorld: lifting and control surfaces, a jet engine, landing gear on a runway and an
// exponential atmosphere. Forces come from the coefficients in LiftingSurface, so stalls, trim and control authority
// all emerge from the simulation rather than being scripted.
//
//   Shift/Ctrl: throttle up/down   Up/Down arrows: stick forward/back   Left/Right arrows: roll
//   A/D: rudder (and nosewheel steering)   B: brakes   C: camera   R: reset   Esc: exit

namespace
{
    constexpr double wheel_height = 1.2;

    sl::LiftingSurface make_surface(const char *control, sl::dvec3 position, sl::dvec3 up, double area, double cl_alpha,
                                    double cl0, double gain)
    {
        sl::LiftingSurface s;
        s.control_name = control;
        s.position = position;
        s.up = up;
        s.area = area;
        s.cl_alpha = cl_alpha;
        s.cl0 = cl0;
        s.control_gain = gain;
        return s;
    }

    int build_aircraft(sl::FlightWorld &world)
    {
        world.flat_ground = true;
        world.uniform_gravity = {0.0, -9.80665, 0.0};
        world.flat_atmosphere.surface_density = 1.225;
        world.integrator = sl::FlightIntegrator::rk4;
        world.max_substep = 0.01;

        const int id = world.create_body("aircraft");
        sl::FlightBody &b = *world.body(id);
        b.dry_mass = 1100.0;
        b.dry_inertia = {2000.0, 2800.0, 1200.0}; // pitch (X), yaw (Y), roll (Z)
        b.tanks.push_back({400.0, 400.0});
        b.drag_area = 0.45;
        b.angular_damping = 150.0;
        b.position = {0.0, wheel_height - 0.07, 400.0};

        // Main wing as two halves so roll rate changes the local angle of attack and damps the roll.
        b.surfaces.push_back(make_surface("wing", {-2.5, 0.0, 0.0}, {0, 1, 0}, 8.0, 4.6, 0.3, 0.0));
        b.surfaces.push_back(make_surface("wing", {2.5, 0.0, 0.0}, {0, 1, 0}, 8.0, 4.6, 0.3, 0.0));
        // Ailerons on the outer wings: positive control rolls right.
        b.surfaces.push_back(make_surface("aileron", {-4.5, 0.0, 0.0}, {0, 1, 0}, 1.4, 3.5, 0.0, 0.18));
        b.surfaces.push_back(make_surface("aileron", {4.5, 0.0, 0.0}, {0, 1, 0}, 1.4, 3.5, 0.0, -0.18));
        // Tailplane with elevator: positive control pushes the nose down.
        b.surfaces.push_back(make_surface("elevator", {0.0, 0.2, 5.0}, {0, 1, 0}, 3.5, 4.0, 0.0, 0.12));
        // Fin and rudder: positive control yaws left.
        b.surfaces.push_back(make_surface("rudder", {0.0, 0.9, 5.0}, {1, 0, 0}, 1.8, 3.0, 0.0, 0.2));

        sl::Thruster jet;
        jet.position = {0.0, 0.0, 1.0};
        jet.direction = {0.0, 0.0, -1.0};
        jet.max_thrust = 4500.0;
        jet.isp = 3000.0;
        jet.tank = 0;
        jet.air_breathing = true;
        b.thrusters.push_back(jet);
        world.build_standard_groups(id);

        const sl::dvec3 wheels[] = {{0.0, -wheel_height, -2.5}, {-1.5, -wheel_height, 0.6}, {1.5, -wheel_height, 0.6}};
        for (const sl::dvec3 &p : wheels)
        {
            sl::GearContact g;
            g.position = p;
            g.stiffness = 6.0e4;
            g.damping = 8.0e3;
            b.gear.push_back(g);
        }
        return id;
    }

    struct Scatter
    {
        float x, z, w, h, d;
    };

    float hash(int n)
    {
        std::uint32_t u = static_cast<std::uint32_t>(n);
        u = (u << 13) ^ u;
        return static_cast<float>(1.0 - ((u * (u * u * 15731u + 789221u) + 1376312589u) & 0x7fffffffu) / 1073741824.0);
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
    sl::set_window_title("simlib - Aircraft Flight (exflight)");

    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 4);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }
    sl::Mesh *cube = sl::create_cube_mesh(1.0f, {255, 255, 255, 255});
    sl::Mesh *ground = sl::create_plane_mesh(20000.0f, 20000.0f, {255, 255, 255, 255});

    std::vector<Scatter> blocks;
    for (int i = 0; i < 400; ++i)
    {
        const float x = hash(i * 3 + 1) * 4000.0f;
        const float z = hash(i * 3 + 2) * 4000.0f;
        if (std::fabs(x) < 120.0f && z > -2000.0f && z < 1000.0f)
            continue;
        blocks.push_back({x, z, 15.0f + 25.0f * std::fabs(hash(i * 7 + 5)), 8.0f + 60.0f * std::fabs(hash(i * 11 + 3)),
                          15.0f + 25.0f * std::fabs(hash(i * 13 + 9))});
    }

    sl::FlightWorld world;
    int id = build_aircraft(world);
    int camera_mode = 0;
    bool running = true;
    std::uint32_t last_time = sl::time_ms();

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
                    world = sl::FlightWorld();
                    id = build_aircraft(world);
                }
                else if (key == sl::Event::Key::letter_c)
                    camera_mode = (camera_mode + 1) % 3;
            }
            sl::display_handle_event(event);
        }

        sl::FlightBody &plane = *world.body(id);
        auto axis = [](sl::Key plus, sl::Key minus)
        { return (sl::key_down(plus) ? 1.0 : 0.0) - (sl::key_down(minus) ? 1.0 : 0.0); };

        double &throttle = plane.thrusters[0].level;
        if (sl::key_down(sl::Key::left_shift))
            throttle = std::min(1.0, throttle + 0.5 * dt);
        if (sl::key_down(sl::Key::left_control))
            throttle = std::max(0.0, throttle - 0.5 * dt);

        const double rudder = axis(sl::Key::letter_a, sl::Key::letter_d);
        world.set_control(id, "elevator", axis(sl::Key::arrow_up, sl::Key::arrow_down));
        world.set_control(id, "aileron", axis(sl::Key::arrow_right, sl::Key::arrow_left));
        world.set_control(id, "rudder", rudder);
        world.set_brakes(id, sl::key_down(sl::Key::letter_b) ? 1.0 : 0.0);
        // Nosewheel steering follows the rudder pedals.
        plane.gear[0].forward = {std::sin(-rudder * 0.5), 0.0, -std::cos(rudder * 0.5)};

        world.step(dt);

        const sl::dvec3 velocity = plane.direction_to_local(plane.velocity);
        const double airspeed = glm::length(velocity);
        const double alpha = airspeed > 1.0 ? std::atan2(-velocity.y, -velocity.z) : 0.0;
        const double altitude = plane.position.y - wheel_height;
        const bool crashed = altitude < -1.0;

        // Draw relative to the aircraft so floats stay precise.
        const glm::mat3 rot = glm::mat3(glm::mat3_cast(glm::quat(plane.orientation)));
        sl::Camera camera;
        camera.field_of_view = 60.0f;
        camera.near_plane = 0.3f;
        camera.far_plane = 12000.0f;
        const glm::vec3 eye_offset[] = {glm::vec3(0.0f, 2.5f, 12.0f), glm::vec3(8.0f, 1.0f, 6.0f), glm::vec3(0.0f, 0.5f, -1.5f)};
        if (camera_mode == 2)
        {
            camera.position = rot * eye_offset[2];
            camera.target = camera.position + rot * glm::vec3(0.0f, 0.0f, -10.0f);
            camera.up = rot * glm::vec3(0.0f, 1.0f, 0.0f);
        }
        else
        {
            // Chase camera that keeps the horizon level and follows the heading.
            const glm::vec3 forward = rot * glm::vec3(0.0f, 0.0f, -1.0f);
            const float heading = std::atan2(forward.x, -forward.z);
            const glm::mat3 yaw_only = glm::mat3(glm::rotate(glm::mat4(1.0f), -heading, glm::vec3(0.0f, 1.0f, 0.0f)));
            camera.position = yaw_only * glm::vec3(eye_offset[camera_mode].x, eye_offset[camera_mode].y + 0.0f, eye_offset[camera_mode].z);
            camera.target = glm::vec3(0.0f);
            camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        const glm::vec3 origin = glm::vec3(plane.position);

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({120, 170, 230, 255});
            sl::Light3D light;
            light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
            light.ambient = 0.35f;
            sl::set_light_3d(light);

            sl::Material grass;
            grass.tint = {70, 120, 60, 255};
            grass.cull = sl::CullMode::none;
            sl::draw_mesh(ground, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f) - origin), grass);

            sl::Material runway;
            runway.tint = {60, 60, 66, 255};
            sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.02f, -300.0f) - origin) *
                                    glm::scale(glm::mat4(1.0f), glm::vec3(30.0f, 0.04f, 1800.0f)),
                          runway);
            runway.tint = {235, 235, 235, 255};
            for (int i = 0; i < 60; ++i)
                sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.06f, 560.0f - i * 30.0f) - origin) *
                                        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 0.04f, 12.0f)),
                              runway);

            sl::Material building;
            for (const Scatter &s : blocks)
            {
                const float shade = 140.0f + 80.0f * std::fabs(hash(static_cast<int>(s.x) + 7));
                building.tint = {static_cast<std::uint8_t>(shade), static_cast<std::uint8_t>(shade * 0.9f),
                                 static_cast<std::uint8_t>(shade * 0.8f), 255};
                sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), glm::vec3(s.x, s.h * 0.5f, s.z) - origin) *
                                        glm::scale(glm::mat4(1.0f), glm::vec3(s.w, s.h, s.d)),
                              building);
            }

            // The aircraft: fuselage, wings, tailplane and fin from boxes.
            sl::Material paint;
            auto part = [&](glm::vec3 position, glm::vec3 size, sl::Colour colour)
            {
                paint.tint = colour;
                sl::draw_mesh(cube, glm::mat4(rot) * glm::translate(glm::mat4(1.0f), position) *
                                        glm::scale(glm::mat4(1.0f), size),
                              paint);
            };
            part({0.0f, 0.0f, 0.0f}, {1.3f, 1.3f, 8.5f}, {230, 230, 235, 255});
            part({0.0f, 0.0f, -4.4f}, {0.8f, 0.8f, 0.6f}, {200, 60, 50, 255});
            part({0.0f, 0.0f, 0.0f}, {11.0f, 0.12f, 1.8f}, {210, 210, 220, 255});
            part({0.0f, 0.2f, 5.0f}, {4.0f, 0.1f, 1.0f}, {210, 210, 220, 255});
            part({0.0f, 0.9f, 5.0f}, {0.1f, 1.5f, 1.2f}, {200, 60, 50, 255});
            for (const sl::GearContact &g : plane.gear)
                part(glm::vec3(g.position) + glm::vec3(0.0f, 0.5f - static_cast<float>(g.compression), 0.0f),
                     {0.25f, 0.5f, 0.5f}, {30, 30, 30, 255});
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {120, 170, 230});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);

        sl::gprintf(16, 12, {20, 20, 30}, "exflight - aircraft simulation%s", crashed ? "   *** CRASHED - press R ***" : "");
        sl::gprintf(16, 34, {20, 20, 60}, "Airspeed %.0f km/h   Altitude %.0f m   Climb %+.1f m/s   AoA %+.1f deg", airspeed * 3.6,
                    altitude, plane.velocity.y, glm::degrees(alpha));
        sl::gprintf(16, 52, {20, 20, 60}, "Throttle %.0f%%   Fuel %.0f kg   Gear %s", plane.thrusters[0].level * 100.0,
                    plane.tanks[0].mass, plane.gear[1].compression > 0.0 ? "on ground" : "airborne");
        if (alpha > 0.25 && airspeed > 10.0)
            sl::gprintf_center(90, {200, 30, 30}, "STALL");
        sl::gprintf_center(screen_h - 22, {20, 20, 40},
                           "[Shift/Ctrl] Throttle  [Up/Down] Pitch  [Left/Right] Roll  [A/D] Rudder  [B] Brakes  [C] Camera  [R] Reset");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::destroy_mesh(cube);
    sl::destroy_mesh(ground);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
