#include "sl.h"
#include "example_fx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// An ocean with Gerstner waves, volumetric clouds and a battleship riding the swell.
//
// The sea is a polar grid of vertices that stays centred near the ship, dense close in and sparse towards the horizon.
// Each frame a sum of Gerstner waves displaces it, and the same wave function is sampled at four points under the hull
// so the ship heaves, pitches and rolls with the water. Small waves fade out with distance so they never alias. The sky
// is the planet atmosphere, so moving the sun produces sunsets, and the clouds are a ray-marched cumulus layer under a
// cirrus layer. Sea, wind and engine sound are synthesised loops whose volume follows the sea state.
//
//   Up/Down: sea state   Left/Right: cloud cover   Q/E: sun elevation   Right-drag: look   [ / ]: zoom
//   P: pause   M: sound   Esc: exit

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr double gravity = 9.80665;
    constexpr int wave_count = 6;

    struct Wave
    {
        double wavelength, steepness, dir_x, dir_z, phase;
        double k, omega;
    };

    // The sea surface: Gerstner waves whose amplitude scales with the sea state.
    class Sea
    {
    public:
        Sea()
        {
            const double lengths[wave_count] = {140.0, 78.0, 44.0, 24.0, 13.0, 7.0};
            const double steepness[wave_count] = {0.11, 0.10, 0.09, 0.08, 0.07, 0.06};
            const double spread[wave_count] = {0.0, 0.35, -0.5, 0.75, -0.9, 1.3};
            for (int i = 0; i < wave_count; ++i)
            {
                Wave &w = waves[i];
                w.wavelength = lengths[i];
                w.steepness = steepness[i];
                w.dir_x = std::cos(0.4 + spread[i]);
                w.dir_z = std::sin(0.4 + spread[i]);
                w.phase = 1.7 * i;
                w.k = 2.0 * pi / w.wavelength;
                w.omega = std::sqrt(gravity * w.k);
            }
        }

        struct Point
        {
            double x, y, z;
            glm::vec3 normal;
            float foam;
        };

        double state = 1.0;

        double amplitude(const Wave &w) const { return state * w.steepness / w.k; }

        /** The displaced surface point for a rest position. `fade` scales each wave (1 = full). */
        Point surface(double px, double pz, double t, const double *fade = nullptr) const
        {
            Point p{px, 0.0, pz, {0.0f, 1.0f, 0.0f}, 0.0f};
            double nx = 0.0, nz = 0.0, pinch = 0.0;
            for (int i = 0; i < wave_count; ++i)
            {
                const Wave &w = waves[i];
                const double f = fade ? fade[i] : 1.0;
                if (f <= 0.0)
                    continue;
                const double a = amplitude(w) * f;
                const double q = std::min(1.0, 0.8 / (wave_count * std::max(w.k * a, 0.1)));
                const double theta = w.k * (w.dir_x * px + w.dir_z * pz) - w.omega * t + w.phase;
                const double c = std::cos(theta), s = std::sin(theta);
                p.x += q * a * w.dir_x * c;
                p.z += q * a * w.dir_z * c;
                p.y += a * s;
                nx -= w.dir_x * w.k * a * c;
                nz -= w.dir_z * w.k * a * c;
                pinch += q * w.k * a * s;
            }
            p.normal = glm::normalize(glm::vec3(static_cast<float>(nx), static_cast<float>(1.0 - pinch), static_cast<float>(nz)));
            p.foam = static_cast<float>(fx::smoothstep(0.35, 0.6, pinch));
            return p;
        }

        /** The water surface at a world position: the rest position is found by inverting the horizontal displacement. */
        Point at(double x, double z, double t) const
        {
            double px = x, pz = z;
            for (int i = 0; i < 3; ++i)
            {
                const Point p = surface(px, pz, t);
                px += x - p.x;
                pz += z - p.z;
            }
            return surface(px, pz, t);
        }

    private:
        Wave waves[wave_count];
    };

    // A polar grid of vertices: one at the centre, then rings that grow geometrically out to the horizon.
    class SeaMesh
    {
    public:
        static constexpr int rings = 150;
        static constexpr int segments = 256;
        static constexpr double inner = 1.5;
        static constexpr double outer = 14000.0;

        bool create()
        {
            vertices.resize(static_cast<std::size_t>(rings + 1) * segments);
            std::vector<std::uint32_t> indices;
            for (int r = 0; r < rings; ++r)
                for (int s = 0; s < segments; ++s)
                {
                    const std::uint32_t a = r * segments + s, b = r * segments + (s + 1) % segments;
                    const std::uint32_t c = (r + 1) * segments + s, d = (r + 1) * segments + (s + 1) % segments;
                    indices.insert(indices.end(), {a, c, b, b, c, d});
                }
            for (sl::Vertex3D &v : vertices)
            {
                v.tangent[0] = 1.0f;
                v.tangent[3] = 1.0f;
            }
            mesh = sl::create_mesh(vertices, indices);
            return mesh != nullptr;
        }

        void destroy()
        {
            if (mesh)
                sl::destroy_mesh(mesh);
            mesh = nullptr;
        }

        /** Rebuild the surface around `centre`, with positions relative to `eye` so floats stay precise. */
        void update(const Sea &sea, double cx, double cz, const sl::dvec3 &eye, double t)
        {
            const double ratio = outer / inner;
            for (int r = 0; r <= rings; ++r)
            {
                const double radius = r == 0 ? 0.0 : inner * std::pow(ratio, static_cast<double>(r - 1) / (rings - 1));
                // Radial spacing of the ring, used to fade waves the grid is too coarse to represent.
                const double cell = std::max(radius * std::log(ratio) / (rings - 1), 0.5);
                double fade[wave_count];
                for (int i = 0; i < wave_count; ++i)
                {
                    const double wavelength = 2.0 * pi / sea_waves_k(sea, i);
                    fade[i] = std::clamp((wavelength / cell - 3.0) / 5.0, 0.0, 1.0);
                }
                for (int s = 0; s < segments; ++s)
                {
                    const double angle = 2.0 * pi * s / segments;
                    const Sea::Point p = sea.surface(cx + radius * std::cos(angle), cz + radius * std::sin(angle), t, fade);
                    sl::Vertex3D &v = vertices[static_cast<std::size_t>(r) * segments + s];
                    v.position[0] = static_cast<float>(p.x - eye.x);
                    v.position[1] = static_cast<float>(p.y - eye.y);
                    v.position[2] = static_cast<float>(p.z - eye.z);
                    v.normal[0] = p.normal.x;
                    v.normal[1] = p.normal.y;
                    v.normal[2] = p.normal.z;
                    // Deep blue-green water, lighter on the crests, with white foam where the surface pinches.
                    const float lift = std::clamp(static_cast<float>(p.y) * 0.12f + 0.4f, 0.0f, 1.0f);
                    const float foam = p.foam * static_cast<float>(fade[0] * 0.6 + 0.4);
                    v.colour[0] = 0.03f + 0.07f * lift + 0.85f * foam;
                    v.colour[1] = 0.16f + 0.26f * lift + 0.85f * foam;
                    v.colour[2] = 0.24f + 0.26f * lift + 0.85f * foam;
                    v.colour[3] = 1.0f;
                }
            }
            sl::update_mesh_vertices(mesh, vertices);
        }

        sl::Mesh *mesh = nullptr;

    private:
        static double sea_waves_k(const Sea &, int i)
        {
            const double lengths[wave_count] = {140.0, 78.0, 44.0, 24.0, 13.0, 7.0};
            return 2.0 * pi / lengths[i];
        }

        std::vector<sl::Vertex3D> vertices;
    };

    // A lofted warship hull, bow along -Z, waterline at y = 0. Below the waterline is red, above is battleship grey.
    sl::Mesh *make_hull(float length, float beam, float draft, float freeboard)
    {
        constexpr int stations = 41;
        constexpr int half = 8; // points from the keel up one side to the deck
        std::vector<sl::Vertex3D> vertices;
        std::vector<std::uint32_t> indices;
        const auto station_width = [&](float t)
        {
            const float bow = std::sin(std::min(t / 0.32f, 1.0f) * 1.5707963f);
            const float stern = 1.0f - 0.35f * fx::smoothstep(0.88, 1.0, t);
            return 0.5f * beam * std::pow(bow, 0.75f) * stern;
        };
        const int across = half * 2 + 2;
        const auto build_row = [&](int station) -> std::vector<glm::vec3>
        {
            const float t = static_cast<float>(station) / (stations - 1);
            const float z = (t - 0.5f) * length;
            const float w = station_width(t);
            // Bow rake: the deck stays full length while the keel rises towards the stem.
            const float keel = -draft * (1.0f - 0.9f * std::pow(1.0f - std::min(t / 0.25f, 1.0f), 2.0f));
            const float sheer = freeboard * (1.0f + 0.25f * std::pow(1.0f - std::min(t / 0.25f, 1.0f), 2.0f));
            std::vector<glm::vec3> row;
            std::vector<glm::vec3> side;
            for (int j = 0; j < half; ++j)
            {
                const float u = static_cast<float>(j) / (half - 1);
                // From the keel centre round the bilge to the topsides.
                const float x = w * std::pow(std::sin(u * 1.5707963f), 1.6f);
                const float y = keel + (sheer - keel) * (j == half - 1 ? 1.0f : std::pow(u, 1.4f) * 0.9f);
                side.push_back({x, y, z});
            }
            for (int j = half - 1; j >= 0; --j)
                row.push_back({-side[j].x, side[j].y, side[j].z});
            for (int j = 0; j < half; ++j)
                row.push_back(side[j]);
            return row;
        };
        // The waterline is a colour change, so build each row with two extra points sharing the position at y = 0.
        std::vector<std::vector<glm::vec3>> grid;
        for (int i = 0; i < stations; ++i)
        {
            std::vector<glm::vec3> row = build_row(i);
            std::vector<glm::vec3> with_line;
            for (std::size_t j = 0; j + 1 < row.size(); ++j)
            {
                with_line.push_back(row[j]);
                const float y0 = row[j].y, y1 = row[j + 1].y;
                if ((y0 < 0.0f) != (y1 < 0.0f))
                {
                    const float f = (0.0f - y0) / (y1 - y0);
                    const glm::vec3 p = row[j] + (row[j + 1] - row[j]) * f;
                    with_line.push_back(p);
                    with_line.push_back(p);
                }
            }
            with_line.push_back(row.back());
            grid.push_back(with_line);
        }
        const int cols = static_cast<int>(grid[0].size());
        for (int i = 0; i < stations; ++i)
            for (int j = 0; j < cols; ++j)
            {
                const glm::vec3 &p = grid[i][j];
                const glm::vec3 &pi_ = grid[std::max(i - 1, 0)][j], &pn = grid[std::min(i + 1, stations - 1)][j];
                const glm::vec3 &pj = grid[i][std::max(j - 1, 0)], &pk = grid[i][std::min(j + 1, cols - 1)];
                glm::vec3 n = glm::cross(pn - pi_, pk - pj);
                if (glm::length(n) < 1e-6f)
                    n = glm::vec3(0.0f, 1.0f, 0.0f);
                n = glm::normalize(n);
                // Orient outward from the hull's centre line.
                if (glm::dot(n, glm::vec3(p.x, p.y + 0.5f * draft, 0.0f)) < 0.0f)
                    n = -n;
                sl::Vertex3D v;
                v.position[0] = p.x;
                v.position[1] = p.y;
                v.position[2] = p.z;
                v.normal[0] = n.x;
                v.normal[1] = n.y;
                v.normal[2] = n.z;
                // The duplicated waterline points: the first of the pair belongs to the red part, the second to the topsides.
                const bool below = p.y < -0.001f || (j > 0 && j + 1 < cols && p.y < 0.001f && grid[i][j - 1].y < p.y - 0.001f);
                const bool upper_dup = std::fabs(p.y) < 0.001f && j > 0 && grid[i][j - 1] == p;
                const bool red = (below || (std::fabs(p.y) < 0.001f && !upper_dup)) && !(upper_dup);
                const float r = red ? 0.42f : 0.36f, g = red ? 0.08f : 0.38f, b = red ? 0.07f : 0.41f;
                v.colour[0] = r;
                v.colour[1] = g;
                v.colour[2] = b;
                vertices.push_back(v);
            }
        for (int i = 0; i + 1 < stations; ++i)
            for (int j = 0; j + 1 < cols; ++j)
            {
                // A zero-width quad between the duplicated points is skipped, keeping the colour break sharp.
                const std::uint32_t a = i * cols + j, b = a + 1, c = (i + 1) * cols + j, d = c + 1;
                indices.insert(indices.end(), {a, b, c, b, d, c});
            }
        // The deck: a flat strip across each station.
        const std::uint32_t deck_start = static_cast<std::uint32_t>(vertices.size());
        for (int i = 0; i < stations; ++i)
        {
            const float t = static_cast<float>(i) / (stations - 1);
            const float sheer = freeboard * (1.0f + 0.25f * std::pow(1.0f - std::min(t / 0.25f, 1.0f), 2.0f));
            const float w = station_width(t) * 0.99f;
            for (int side = 0; side < 2; ++side)
            {
                sl::Vertex3D v;
                v.position[0] = side ? w : -w;
                v.position[1] = sheer;
                v.position[2] = (t - 0.5f) * length;
                v.normal[1] = 1.0f;
                v.colour[0] = 0.42f;
                v.colour[1] = 0.33f;
                v.colour[2] = 0.22f;
                vertices.push_back(v);
            }
        }
        for (int i = 0; i + 1 < stations; ++i)
        {
            const std::uint32_t a = deck_start + i * 2;
            indices.insert(indices.end(), {a, a + 2, a + 1, a + 1, a + 2, a + 3});
        }
        (void)across;
        return sl::create_mesh(vertices, indices);
    }

    struct Ship
    {
        sl::dvec3 position{0.0, 0.0, 0.0};
        double heading = 0.0; // radians, 0 = towards -Z
        double heave = 0.0, heave_velocity = 0.0;
        double pitch = 0.0, pitch_velocity = 0.0;
        double roll = 0.0, roll_velocity = 0.0;

        static constexpr double length = 130.0;
        static constexpr double beam = 20.0;
        static constexpr double draft = 7.0;
        static constexpr double freeboard = 6.0;

        glm::dvec3 forward() const { return {std::sin(heading), 0.0, -std::cos(heading)}; }
        glm::dvec3 right() const { return {std::cos(heading), 0.0, std::sin(heading)}; }

        // A damped spring pulls each degree of freedom towards what the water under the hull is doing.
        static void follow(double &value, double &velocity, double target, double omega, double damping, double dt)
        {
            const double accel = omega * omega * (target - value) - 2.0 * damping * omega * velocity;
            velocity += accel * dt;
            value += velocity * dt;
        }

        void update(const Sea &sea, double t, double dt)
        {
            heading += 0.012 * dt;
            position += forward() * 8.0 * dt;
            const glm::dvec3 f = forward(), r = right();
            const auto height = [&](double along, double across)
            { return sea.at(position.x + f.x * along + r.x * across, position.z + f.z * along + r.z * across, t).y; };
            const double bow = height(length * 0.4, 0.0), stern = height(-length * 0.4, 0.0);
            const double port = height(0.0, -beam * 0.5), starboard = height(0.0, beam * 0.5);
            const double centre = height(0.0, 0.0);
            const double heave_target = 0.4 * centre + 0.15 * (bow + stern) + 0.15 * (port + starboard);
            const double pitch_target = std::atan2(bow - stern, length * 0.8);
            const double roll_target = std::atan2(starboard - port, beam);
            const int steps = 4;
            for (int i = 0; i < steps; ++i)
            {
                const double h = dt / steps;
                follow(heave, heave_velocity, heave_target, 1.4, 0.45, h);
                follow(pitch, pitch_velocity, pitch_target, 1.2, 0.5, h);
                follow(roll, roll_velocity, roll_target, 0.9, 0.35, h);
            }
        }

        glm::mat3 rotation() const
        {
            const glm::mat4 m = glm::rotate(glm::mat4(1.0f), static_cast<float>(-heading), glm::vec3(0, 1, 0)) *
                                glm::rotate(glm::mat4(1.0f), static_cast<float>(pitch), glm::vec3(1, 0, 0)) *
                                glm::rotate(glm::mat4(1.0f), static_cast<float>(roll), glm::vec3(0, 0, 1));
            return glm::mat3(m);
        }
    };

    struct Smoke
    {
        sl::dvec3 position;
        glm::vec3 drift;
        float age, life, size;
    };

    struct SeaSound
    {
        sl::Sample *wind = nullptr, *surf = nullptr, *engine = nullptr;
        std::uint64_t wind_voice = 0, surf_voice = 0, engine_voice = 0;

        void start()
        {
            wind = fx::make_wind_sound();
            surf = fx::make_hiss_sound();
            engine = fx::make_jet_idle_sound();
            wind_voice = wind ? sl::play_sample(wind, 0, 128, 1000, -1) : 0;
            surf_voice = surf ? sl::play_sample(surf, 0, 128, 1000, -1) : 0;
            engine_voice = engine ? sl::play_sample(engine, 0, 128, 1000, -1) : 0;
        }

        void set(double state, bool on)
        {
            const auto level = [&](std::uint64_t voice, double v)
            {
                if (voice)
                    sl::set_voice_volume(voice, static_cast<int>(std::clamp(on ? v : 0.0, 0.0, 1.0) * 255.0));
            };
            level(wind_voice, 0.12 + 0.4 * state);
            level(surf_voice, 0.05 + 0.2 * state);
            level(engine_voice, 0.10);
        }

        void stop()
        {
            sl::stop_all_samples();
            for (sl::Sample **s : {&wind, &surf, &engine})
            {
                if (*s)
                    sl::destroy_sample(*s);
                *s = nullptr;
            }
            wind_voice = surf_voice = engine_voice = 0;
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
    sl::set_window_title("simlib - Ocean (exocean)");

    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 4);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    SeaMesh sea_mesh;
    sl::Mesh *cube = sl::create_cube_mesh(1.0f, {255, 255, 255, 255});
    sl::Mesh *puff = sl::create_sphere_mesh(1.0f, 12, 8, {255, 255, 255, 255});
    sl::Mesh *hull = make_hull(static_cast<float>(Ship::length), static_cast<float>(Ship::beam),
                               static_cast<float>(Ship::draft), static_cast<float>(Ship::freeboard));
    if (!sea_mesh.create() || !cube || !puff || !hull)
    {
        std::fprintf(stderr, "Failed to create meshes: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    Sea sea;
    Ship ship;
    std::vector<Smoke> smoke;
    SeaSound sound;
    sound.start();
    bool sound_on = true, paused = false;
    float cam_yaw = static_cast<float>(pi) + 0.55f, cam_pitch = 0.22f;
    double cam_distance = 190.0;
    double sun_elevation = 24.0; // degrees
    float coverage = 0.55f;
    double clock = 0.0, smoke_debt = 0.0;
    std::uint32_t rng = 99u;
    const auto random01 = [&rng]
    {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>(rng >> 8) / 16777216.0f - 0.5f;
    };
    int last_mouse_x = 0, last_mouse_y = 0;
    bool running = true;
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
                    paused = !paused;
                else if (key == sl::Event::Key::letter_m)
                    sound_on = !sound_on;
            }
            sl::display_handle_event(event);
        }

        if (sl::key_down(sl::Key::arrow_up))
            sea.state = std::min(2.2, sea.state + 0.5 * frame);
        if (sl::key_down(sl::Key::arrow_down))
            sea.state = std::max(0.1, sea.state - 0.5 * frame);
        if (sl::key_down(sl::Key::arrow_right))
            coverage = std::min(1.0f, coverage + 0.3f * static_cast<float>(frame));
        if (sl::key_down(sl::Key::arrow_left))
            coverage = std::max(0.0f, coverage - 0.3f * static_cast<float>(frame));
        if (sl::key_down(sl::Key::letter_q))
            sun_elevation = std::min(80.0, sun_elevation + 12.0 * frame);
        if (sl::key_down(sl::Key::letter_e))
            sun_elevation = std::max(-3.0, sun_elevation - 12.0 * frame);
        if (sl::key_down(sl::Key::left_bracket))
            cam_distance = std::max(50.0, cam_distance / (1.0 + 1.5 * frame));
        if (sl::key_down(sl::Key::right_bracket))
            cam_distance = std::min(1500.0, cam_distance * (1.0 + 1.5 * frame));
        const int mx = sl::mouse_x(), my = sl::mouse_y();
        if (sl::mouse_button_down(sl::MouseButton::right))
        {
            cam_yaw -= static_cast<float>(mx - last_mouse_x) * 0.008f;
            cam_pitch = std::clamp(cam_pitch + static_cast<float>(my - last_mouse_y) * 0.008f, 0.02f, 1.45f);
        }
        last_mouse_x = mx;
        last_mouse_y = my;

        if (!paused)
        {
            clock += frame;
            ship.update(sea, clock, frame);
        }
        const double dt = paused ? 0.0 : frame;

        // Funnel smoke drifts downwind and thins as it ages.
        const glm::mat3 ship_rot = ship.rotation();
        const sl::dvec3 ship_pos(ship.position.x, ship.heave, ship.position.z);
        const float fdt = static_cast<float>(dt);
        for (Smoke &p : smoke)
        {
            p.age += fdt;
            p.position += sl::dvec3(p.drift) * dt;
        }
        smoke.erase(std::remove_if(smoke.begin(), smoke.end(), [](const Smoke &p) { return p.age >= p.life; }), smoke.end());
        smoke_debt += dt * 30.0;
        for (; smoke_debt >= 1.0; smoke_debt -= 1.0)
        {
            static int funnel_index = 0;
            funnel_index ^= 1;
            const glm::vec3 funnel_top = ship_rot * glm::vec3(0.0f, static_cast<float>(Ship::freeboard) + 14.5f, funnel_index ? -6.0f : 8.0f);
            smoke.push_back({ship_pos + sl::dvec3(funnel_top) + sl::dvec3(random01(), random01(), random01()) * 0.5,
                             {3.0f + random01(), 1.8f + random01(), 1.0f + random01()}, 0.0f, 9.0f + 3.0f * random01(),
                             2.0f + 0.8f * (random01() + 0.5f)});
        }

        // The eye orbits the ship and is kept above the waves.
        const double angle = ship.heading + cam_yaw;
        const glm::vec3 offset = static_cast<float>(cam_distance) *
                                 glm::vec3(std::cos(cam_pitch) * std::sin(angle), std::sin(cam_pitch), std::cos(cam_pitch) * std::cos(angle));
        const sl::dvec3 focus = ship_pos + sl::dvec3(0.0, 4.0, 0.0);
        sl::dvec3 eye = focus + sl::dvec3(offset);
        eye.y = std::max(eye.y, sea.at(eye.x, eye.z, clock).y + 2.0 + 0.012 * cam_distance);
        const glm::vec3 look = glm::vec3(focus - eye);

        sea_mesh.update(sea, ship.position.x + 0.35 * (eye.x - ship.position.x), ship.position.z + 0.35 * (eye.z - ship.position.z),
                        eye, clock);

        sl::Camera camera;
        camera.position = glm::vec3(0.0f);
        camera.target = look;
        camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
        camera.field_of_view = 55.0f;
        camera.near_plane = 0.5f;
        camera.far_plane = 40000.0f;

        const double elevation = glm::radians(sun_elevation);
        sl::Environment3D environment;
        environment.sun_direction = glm::normalize(glm::vec3(-0.55f * std::cos(elevation), -std::sin(elevation), -0.83f * std::cos(elevation)));
        environment.sun_intensity = 20.0f;
        environment.up = glm::vec3(0.0f, 1.0f, 0.0f);
        environment.camera_altitude = static_cast<float>(eye.y);
        environment.camera_position = eye;
        environment.time = clock;
        environment.planet = true;
        environment.haze_scale = 0.5f;
        environment.ground_albedo = {0.02f, 0.09f, 0.15f};
        environment.fog_enabled = true;
        environment.fog_density = 4.0e-5f;
        environment.clouds = sl::cloud_layer(sl::CloudType::cumulus);
        environment.clouds.altitude = 1400.0f;
        environment.clouds.coverage = coverage;
        environment.clouds.wind = {9.0f, 5.0f};
        environment.more_clouds[0] = sl::cloud_layer(sl::CloudType::cirrus);
        environment.more_clouds[0].coverage = 0.5f * coverage + 0.1f;
        environment.more_clouds[0].wind = {20.0f, 12.0f};

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({120, 160, 200, 255});
            sl::set_environment_3d(environment);
            sl::draw_sky_3d();
            sl::Light3D light;
            light.direction = environment.sun_direction;
            light.colour = sl::atmosphere_sun_colour(environment);
            light.ambient = 0.28f;
            sl::set_light_3d(light);

            sl::Material water;
            water.pbr = true;
            water.metallic = 0.0f;
            water.roughness = 0.14f;
            water.cull = sl::CullMode::none;
            sl::draw_mesh(sea_mesh.mesh, glm::mat4(1.0f), water);

            const glm::vec3 ship_rel = glm::vec3(ship_pos - eye);
            const glm::mat4 base = glm::translate(glm::mat4(1.0f), ship_rel) * glm::mat4(ship_rot);
            sl::Material paint;
            paint.cull = sl::CullMode::none;
            sl::draw_mesh(hull, base, paint);
            paint.cull = sl::CullMode::back;
            const auto block = [&](glm::vec3 centre, glm::vec3 size, sl::Colour colour)
            {
                paint.tint = colour;
                sl::draw_mesh(cube, base * glm::translate(glm::mat4(1.0f), centre) * glm::scale(glm::mat4(1.0f), size), paint);
            };
            const float deck = static_cast<float>(Ship::freeboard);
            const sl::Colour grey{122, 128, 135, 255}, dark{54, 58, 64, 255}, deck_grey{96, 100, 104, 255};
            // Main battery: two pairs of superfiring turrets, the inner one of each pair raised on a barbette.
            const auto turret = [&](float z, float lift, float dir)
            {
                if (lift > 0.0f)
                    block({0.0f, deck + lift * 0.5f, z}, {9.0f, lift, 9.0f}, deck_grey);
                const float y = deck + lift;
                block({0.0f, y + 1.6f, z}, {9.5f, 3.2f, 9.5f}, grey);
                block({0.0f, y + 3.3f, z + dir * 0.6f}, {8.0f, 0.5f, 7.5f}, deck_grey);
                for (int barrel = -1; barrel <= 1; barrel += 2)
                    block({barrel * 1.7f, y + 2.0f, z + dir * 10.0f}, {0.8f, 0.8f, 13.0f}, dark);
            };
            turret(-52.0f, 0.0f, -1.0f);
            turret(-39.0f, 3.2f, -1.0f);
            turret(37.0f, 3.2f, 1.0f);
            turret(50.0f, 0.0f, 1.0f);
            // Wing turrets amidships.
            for (int side = -1; side <= 1; side += 2)
            {
                block({side * 7.0f, deck + 1.4f, 14.0f}, {5.5f, 2.8f, 6.5f}, grey);
                block({side * 7.0f, deck + 1.8f, 7.0f}, {0.7f, 0.7f, 8.0f}, dark);
            }
            // Conning tower and bridge, with a tripod foremast carrying the fire-control top.
            block({0.0f, deck + 3.5f, -24.0f}, {11.0f, 7.0f, 10.0f}, grey);
            block({0.0f, deck + 8.0f, -25.0f}, {9.0f, 2.2f, 8.0f}, grey);
            block({0.0f, deck + 8.4f, -29.2f}, {8.2f, 0.9f, 0.3f}, dark);
            block({0.0f, deck + 14.0f, -22.0f}, {0.9f, 12.0f, 0.9f}, dark);
            block({0.0f, deck + 18.0f, -22.0f}, {5.0f, 2.2f, 4.0f}, grey);
            block({0.0f, deck + 23.0f, -22.0f}, {0.5f, 9.0f, 0.5f}, dark);
            // Two funnels and a mainmast, with boats on the superstructure between.
            for (float z : {-6.0f, 8.0f})
            {
                block({0.0f, deck + 7.0f, z}, {6.0f, 14.0f, 5.5f}, {60, 62, 66, 255});
                block({0.0f, deck + 13.7f, z}, {6.2f, 1.2f, 5.7f}, {20, 20, 22, 255});
            }
            block({0.0f, deck + 2.0f, 1.0f}, {10.0f, 4.0f, 6.0f}, grey);
            block({0.0f, deck + 12.0f, 22.0f}, {0.6f, 24.0f, 0.6f}, dark);
            block({0.0f, deck + 18.0f, 22.0f}, {6.0f, 0.3f, 0.3f}, dark);
            // Casemate guns along the sides.
            for (int side = -1; side <= 1; side += 2)
                for (int i = 0; i < 8; ++i)
                    block({side * (static_cast<float>(Ship::beam) * 0.5f - 0.2f), deck - 1.2f, -20.0f + i * 5.0f}, {0.5f, 1.2f, 1.6f}, dark);

            sl::Material haze;
            haze.lit = false;
            haze.blend = sl::MaterialBlend::alpha;
            for (const Smoke &p : smoke)
            {
                const float age = p.age / p.life;
                const float size = p.size * (1.0f + 5.0f * age);
                const float alpha = 120.0f * (1.0f - age) * std::min(1.0f, age * 6.0f + 0.2f);
                haze.tint = {70, 70, 74, static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 255.0f))};
                sl::draw_mesh(puff, glm::translate(glm::mat4(1.0f), glm::vec3(p.position - eye)) * glm::scale(glm::mat4(1.0f), glm::vec3(size)), haze);
            }
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {120, 160, 200});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);

        sound.set(sea.state * 0.8, sound_on);

        double wave_height = 0.0;
        for (int i = 0; i < 2; ++i)
            wave_height += 2.0 * sea.amplitude(Wave{i ? 78.0 : 140.0, i ? 0.10 : 0.11, 0, 0, 0, 2.0 * pi / (i ? 78.0 : 140.0), 0});
        sl::gprintf(16, 12, {255, 255, 255}, "exocean - ocean, clouds and a dreadnought%s", paused ? "   (paused)" : "");
        sl::gprintf(16, 34, {200, 225, 255}, "Sea state %.1f   Swell about %.1f m crest to trough", sea.state, wave_height);
        sl::gprintf(16, 52, {200, 225, 255}, "Ship heave %+.1f m   Pitch %+.1f deg   Roll %+.1f deg", ship.heave, glm::degrees(ship.pitch),
                    glm::degrees(ship.roll));
        sl::gprintf(16, 70, {200, 225, 255}, "Sun %.0f deg   Cloud cover %.0f%%", sun_elevation, coverage * 100.0f);
        sl::gprintf_center(screen_h - 22, {235, 240, 250},
                           "[Up/Down] Sea state  [Left/Right] Clouds  [Q/E] Sun  [R-drag] Look  [ [ ] ] Zoom  [P] Pause  [M] Sound");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sound.stop();
    sea_mesh.destroy();
    for (sl::Mesh *m : {cube, puff, hull})
        sl::destroy_mesh(m);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
