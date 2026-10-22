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

// Aircraft flight with FlightWorld: lifting and control surfaces, a jet engine, landing gear on a runway and an
// exponential atmosphere. Forces come from the coefficients in LiftingSurface, so stalls, trim and control authority
// all emerge from the simulation rather than being scripted.
//
// The world is a streaming terrain with detail textures, hills, forests, an atmosphere and clouds; the aircraft is a
// lathe-turned jet that trails exhaust smoke, and its engine and the wind are synthesised sound loops that follow the
// throttle and airspeed. The terrain is also the ground for the flight model, so the gear rolls over it and hills are solid.
//
//   Shift/Ctrl: throttle up/down   Up/Down arrows: stick forward/back   Left/Right arrows: roll
//   A/D: rudder (and nosewheel steering)   B: brakes   C: camera   M: sound   R: reset   Esc: exit

namespace
{
    constexpr double wheel_height = 1.2;
    // The airfield terrain sits this far below the runway slab's top surface, which keeps the two from z-fighting. The
    // aircraft model is drawn raised by the same amount so its wheels rest on the slab.
    constexpr double airfield_height = -0.5;

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

    int build_aircraft(sl::FlightWorld &world, const sl::Terrain &terrain)
    {
        world.flat_ground = true;
        world.flat_terrain = &terrain;
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
        b.position = {0.0, airfield_height + wheel_height - 0.07, 400.0};

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

    struct Block
    {
        float x, z, w, h, d;
    };

    struct Puff
    {
        sl::dvec3 position;
        glm::vec3 drift;
        float age, life, size;
    };

    float hash(int n)
    {
        std::uint32_t u = static_cast<std::uint32_t>(n);
        u = (u << 13) ^ u;
        return static_cast<float>(1.0 - ((u * (u * u * 15731u + 789221u) + 1376312589u) & 0x7fffffffu) / 1073741824.0);
    }

    // Distance from the airfield rectangle, used to keep it flat and clear of trees.
    double airfield_distance(double x, double z)
    {
        const double dx = std::max(std::fabs(x) - 15.0, 0.0);
        const double dz = std::max(std::fabs(z + 300.0) - 900.0, 0.0);
        return std::sqrt(dx * dx + dz * dz);
    }

    void settle(sl::Terrain &terrain, sl::ClutterLayer &clutter, const sl::TerrainView &view)
    {
        for (int attempt = 0; attempt < 400; ++attempt)
        {
            terrain.update(view);
            clutter.update(view);
            if (terrain.stats().pending_chunks == 0)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    struct EngineSound
    {
        sl::Sample *idle = nullptr, *power = nullptr, *wind = nullptr;
        std::uint64_t idle_voice = 0, power_voice = 0, wind_voice = 0;

        void start()
        {
            idle = fx::make_jet_idle_sound();
            power = fx::make_jet_power_sound();
            wind = fx::make_wind_sound();
            idle_voice = idle ? sl::play_sample(idle, 0, 128, 1000, -1) : 0;
            power_voice = power ? sl::play_sample(power, 0, 128, 1000, -1) : 0;
            wind_voice = wind ? sl::play_sample(wind, 0, 128, 1000, -1) : 0;
        }

        void set(double throttle, double airspeed, bool on)
        {
            const double gain = on ? 1.0 : 0.0;
            const auto level = [&](std::uint64_t voice, double v)
            {
                if (voice)
                    sl::set_voice_volume(voice, static_cast<int>(std::clamp(v * gain, 0.0, 1.0) * 255.0));
            };
            level(idle_voice, 0.45 * (1.0 - 0.7 * throttle));
            level(power_voice, 0.15 + 0.6 * throttle);
            level(wind_voice, std::min(1.0, airspeed / 90.0) * 0.5);
        }

        void stop()
        {
            sl::stop_all_samples();
            for (sl::Sample **s : {&idle, &power, &wind})
            {
                if (*s)
                    sl::destroy_sample(*s);
                *s = nullptr;
            }
            idle_voice = power_voice = wind_voice = 0;
            sl::audio_fx_shutdown();
        }
    };
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
    sl::Mesh *sphere = sl::create_sphere_mesh(1.0f, 16, 10, {255, 255, 255, 255});
    fx::Jet jet;
    if (!cube || !sphere || !jet.create())
    {
        std::fprintf(stderr, "Failed to create meshes: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    const std::vector<Block> blocks = {{-95.0f, 380.0f, 34.0f, 12.0f, 44.0f}, {-95.0f, 300.0f, 34.0f, 12.0f, 44.0f},
                                       {-95.0f, 220.0f, 34.0f, 14.0f, 44.0f}, {95.0f, 120.0f, 22.0f, 10.0f, 90.0f},
                                       {70.0f, 260.0f, 8.0f, 32.0f, 8.0f},    {95.0f, -40.0f, 30.0f, 9.0f, 40.0f},
                                       {-95.0f, -80.0f, 30.0f, 9.0f, 40.0f}};

    // Streaming terrain: noise hills, flattened around the airfield, with detail textures and a macro texture.
    sl::NoiseParameters noise;
    noise.seed = 24;
    noise.frequency = 1.0 / 2200.0;
    noise.amplitude = 90.0;
    noise.octaves = 4;
    noise.gain = 0.45;
    auto base = std::make_shared<sl::NoiseSource>(noise);
    auto height = [base](double x, double z)
    {
        const double d = airfield_distance(x, z);
        const double n = base->height(x, z);
        const double rolling = fx::smoothstep(250.0, 1400.0, d);
        const double hills = fx::smoothstep(600.0, 2200.0, d);
        return airfield_height + rolling * n + hills * (260.0 + 2.2 * n);
    };
    sl::TerrainConfig config;
    config.size = 16384.0;
    config.origin = {-8192.0, 0.0, -8192.0};
    config.finest_cell_size = 4.0;
    config.max_screen_error = 4.0;
    std::vector<sl::Bitmap *> textures;
    config.layers = fx::detailed_layers(-400.0, 900.0, textures);
    std::unique_ptr<sl::Terrain> terrain;
    std::unique_ptr<sl::ClutterLayer> clutter;
    try
    {
        terrain = std::make_unique<sl::Terrain>(config, std::make_shared<sl::FunctionSource>(height, -400.0, 900.0));
        try
        {
            sl::TerrainTextureOptions options;
            options.fade_start = 150.0;
            options.fade_end = 1200.0;
            options.macro_texture = textures.back();
            options.macro_tile_metres = 1600.0;
            options.macro_strength = 0.4;
            terrain->set_layer_textures(options);
        }
        catch (const std::exception &)
        {
            // Backends without texture arrays keep the plain colour layers.
        }
        clutter = std::make_unique<sl::ClutterLayer>(*terrain);
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "Terrain unavailable: %s\n", e.what());
        sl::shutdown();
        return -1;
    }
    for (sl::Bitmap *bitmap : textures)
        sl::destroy_bitmap(bitmap);
    textures.clear();

    const auto off_airfield = [](const sl::TerrainSample &sample)
    { return airfield_distance(sample.position.x, sample.position.z) > 220.0 ? 1.0f : 0.0f; };
    sl::ClutterSpecies trees;
    trees.mesh = sl::make_conifer_data(8.0f, 1.8f);
    trees.density = 0.0008f;
    trees.max_height = 400.0f;
    trees.max_slope = 32.0f;
    trees.near_distance = 450.0f;
    trees.far_distance = 3200.0f;
    trees.suitability = off_airfield;
    sl::ClutterSpecies rocks;
    rocks.mesh = sl::make_rock_data(1.2f);
    rocks.density = 0.0005f;
    rocks.max_slope = 45.0f;
    rocks.near_distance = 300.0f;
    rocks.far_distance = 1600.0f;
    rocks.seed = 9;
    rocks.suitability = off_airfield;
    clutter->add_species(trees);
    clutter->add_species(rocks);

    sl::FlightWorld world;
    int id = build_aircraft(world, *terrain);
    std::vector<Puff> smoke;
    EngineSound engine;
    engine.start();
    bool sound_on = true;
    float smoke_debt = 0.0f;
    std::uint32_t rng = 12345u;
    const auto random01 = [&rng]
    {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>(rng >> 8) / 16777216.0f;
    };
    int camera_mode = 0;
    bool running = true;
    bool first_frame = true;
    double clock = 0.0;
    std::uint32_t last_time = sl::time_ms();

    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const double dt = std::clamp(static_cast<double>(now - last_time) * 0.001, 0.001, 0.1);
        last_time = now;
        clock += dt;

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
                    id = build_aircraft(world, *terrain);
                    smoke.clear();
                }
                else if (key == sl::Event::Key::letter_c)
                    camera_mode = (camera_mode + 1) % 3;
                else if (key == sl::Event::Key::letter_m)
                    sound_on = !sound_on;
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
        const double ground = terrain->height_at(plane.position.x, plane.position.z);
        const double altitude = plane.position.y - wheel_height - ground;
        const bool crashed = altitude < -1.0;
        engine.set(throttle, airspeed, sound_on);

        const glm::mat3 rot = glm::mat3(glm::mat3_cast(glm::quat(plane.orientation)));

        // Exhaust smoke: puffs are born at the nozzle in world space, so they hang in the air as the aircraft moves on.
        const float fdt = static_cast<float>(dt);
        smoke_debt += fdt * static_cast<float>(6.0 + 90.0 * throttle);
        const sl::dvec3 nozzle = plane.position + sl::dvec3(rot * glm::vec3(0.0f, 0.0f, 4.8f));
        for (; smoke_debt >= 1.0f; smoke_debt -= 1.0f)
        {
            const float life = 3.5f + 1.5f * random01();
            smoke.push_back({nozzle, {(random01() - 0.5f) * 0.6f, 0.15f + 0.3f * random01(), (random01() - 0.5f) * 0.6f},
                             0.0f, life, 0.35f + 0.2f * random01()});
        }
        for (Puff &p : smoke)
        {
            p.age += fdt;
            p.position += sl::dvec3(p.drift) * dt;
        }
        smoke.erase(std::remove_if(smoke.begin(), smoke.end(), [](const Puff &p) { return p.age >= p.life; }), smoke.end());

        // Chase camera that keeps the horizon level and follows the heading; everything is drawn relative to the eye.
        const glm::vec3 eye_offset[] = {glm::vec3(0.0f, 2.5f, 12.0f), glm::vec3(8.0f, 1.0f, 6.0f), glm::vec3(0.0f, 0.5f, -1.5f)};
        glm::vec3 chase;
        glm::vec3 look;
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (camera_mode == 2)
        {
            chase = rot * eye_offset[2];
            look = rot * glm::vec3(0.0f, 0.0f, -10.0f);
            up = rot * up;
        }
        else
        {
            const glm::vec3 forward = rot * glm::vec3(0.0f, 0.0f, -1.0f);
            const float heading = std::atan2(forward.x, -forward.z);
            const glm::mat3 yaw_only = glm::mat3(glm::rotate(glm::mat4(1.0f), -heading, glm::vec3(0.0f, 1.0f, 0.0f)));
            chase = yaw_only * eye_offset[camera_mode];
            look = -chase;
        }
        const sl::dvec3 eye = plane.position + sl::dvec3(chase);
        const glm::vec3 model_origin = glm::vec3(plane.position - eye);

        sl::TerrainView view;
        view.position = eye;
        view.forward = glm::normalize(sl::dvec3(look));
        view.up = sl::dvec3(up);
        view.fov_y_degrees = 60.0;
        view.aspect = static_cast<double>(screen_w) / screen_h;
        view.viewport_height = screen_h;
        view.far_distance = 18000.0;
        if (first_frame)
            settle(*terrain, *clutter, view);
        else
        {
            terrain->update(view);
            clutter->update(view);
        }
        first_frame = false;

        sl::Camera camera;
        camera.field_of_view = 60.0f;
        camera.near_plane = 0.5f;
        camera.far_plane = static_cast<float>(view.far_distance);
        camera.position = glm::vec3(0.0f);
        camera.target = look;
        camera.up = up;

        sl::Environment3D environment;
        environment.sun_direction = glm::normalize(glm::vec3(-0.5f, -0.6f, -0.3f));
        environment.camera_altitude = static_cast<float>(eye.y);
        environment.camera_position = eye;
        environment.time = clock;
        environment.fog_density = 0.00008f;
        environment.clouds.enabled = true;
        environment.clouds.altitude = 900.0f;
        environment.clouds.scale = 3000.0f;
        environment.clouds.coverage = 0.7f;

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({130, 180, 225, 255});
            sl::Light3D light;
            light.direction = environment.sun_direction;
            light.ambient = 0.35f;
            sl::set_environment_3d(environment);
            sl::draw_sky_3d();
            light.colour = sl::atmosphere_sun_colour(environment);
            sl::set_light_3d(light);

            terrain->draw(eye);
            clutter->draw(eye);

            const glm::vec3 eye_f = glm::vec3(eye);
            sl::Material tarmac;
            tarmac.tint = {60, 60, 66, 255};
            sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.4f, -300.0f) - eye_f) *
                                    glm::scale(glm::mat4(1.0f), glm::vec3(30.0f, 0.8f, 1800.0f)),
                          tarmac);
            tarmac.tint = {235, 235, 235, 255};
            for (int i = 0; i < 60; ++i)
                sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.05f, 560.0f - i * 30.0f) - eye_f) *
                                        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 0.1f, 12.0f)),
                              tarmac);

            sl::Material building;
            for (const Block &b : blocks)
            {
                const float shade = 140.0f + 80.0f * std::fabs(hash(static_cast<int>(b.x) + 7));
                building.tint = {static_cast<std::uint8_t>(shade), static_cast<std::uint8_t>(shade * 0.9f),
                                 static_cast<std::uint8_t>(shade * 0.8f), 255};
                sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), glm::vec3(b.x, b.h * 0.5f + airfield_height, b.z) - eye_f) *
                                        glm::scale(glm::mat4(1.0f), glm::vec3(b.w, b.h, b.d)),
                              building);
            }

            std::vector<glm::vec3> wheels;
            for (const sl::GearContact &g : plane.gear)
                wheels.push_back(glm::vec3(g.position) + glm::vec3(0.0f, 0.5f - static_cast<float>(g.compression), 0.0f));
            jet.draw(glm::translate(glm::mat4(1.0f), model_origin) * glm::mat4(rot), wheels);

            sl::Material wisp;
            wisp.lit = false;
            wisp.fog = false;
            wisp.blend = sl::MaterialBlend::alpha;
            for (const Puff &p : smoke)
            {
                const float age = p.age / p.life;
                const float size = p.size * (1.0f + 3.5f * age);
                const float alpha_f = 55.0f * (1.0f - age) * (1.0f - age) * std::min(1.0f, age * 8.0f + 0.2f);
                wisp.tint = {170, 172, 178, static_cast<std::uint8_t>(std::clamp(alpha_f, 0.0f, 255.0f))};
                sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), glm::vec3(p.position - eye)) *
                                          glm::scale(glm::mat4(1.0f), glm::vec3(size)),
                              wisp);
            }
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
                           "[Shift/Ctrl] Throttle  [Up/Down] Pitch  [Left/Right] Roll  [A/D] Rudder  [B] Brakes  [C] Camera  [M] Sound  [R] Reset");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    engine.stop();
    clutter.reset();
    terrain.reset();
    jet.destroy();
    sl::destroy_mesh(cube);
    sl::destroy_mesh(sphere);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
