// exasteroids: classic Asteroids built from neon voxels. The play field is the familiar wrapping 2D one, but the ship
// and every rock are 3D voxel lumps. A shot rock splits into two or three smaller lumps and throws a shower of
// individual voxels that tumble, drift and fade away. Only the lumps are a hazard; the voxel shower is harmless.
//
// Left/Right turn, Up thrusts, Space fires, Enter starts or restarts, Escape quits.
// Options: --autoplay (a bot plays), --shot FILE and --shot-at SECONDS (save a screenshot then quit).
#include "sl.h"

#include "audio_synth.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    constexpr int window_w = 960, window_h = 720;
    constexpr float half_w = 61.0f, half_h = 50.0f; // wrapping play field, a little taller than the tilted camera's view so wrapping happens off screen
    constexpr float dom_w = half_w * 2.0f, dom_h = half_h * 2.0f;
    constexpr float pi = 3.14159265f;

    enum Palette { cyan_c, magenta_c, lime_c, orange_c, yellow_c, violet_c, ship_c, bullet_c, palette_count };
    const sl::Colour palette[palette_count] = {{0, 255, 255},  {255, 30, 220}, {110, 255, 30}, {255, 130, 10},
                                               {255, 240, 30}, {150, 80, 255}, {60, 210, 255}, {255, 255, 140}};

    // Rock sizes: big, medium, small.
    constexpr float level_radius[3] = {7.0f, 4.0f, 2.3f};
    constexpr float level_voxel[3] = {1.0f, 0.75f, 0.55f};
    constexpr int level_points[3] = {20, 50, 100};
    constexpr int level_dust[3] = {150, 90, 45};

    float uniform() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
    float between(float a, float b) { return a + (b - a) * uniform(); }
    float wrap_delta(float d, float size) { return d - size * std::round(d / size); }
    glm::vec2 wrap_delta(glm::vec2 d) { return {wrap_delta(d.x, dom_w), wrap_delta(d.y, dom_h)}; }
    glm::vec2 wrap_pos(glm::vec2 p)
    {
        if (p.x > half_w)
            p.x -= dom_w;
        else if (p.x < -half_w)
            p.x += dom_w;
        if (p.y > half_h)
            p.y -= dom_h;
        else if (p.y < -half_h)
            p.y += dom_h;
        return p;
    }
    glm::vec3 random_unit()
    {
        glm::vec3 v;
        do
            v = {between(-1, 1), between(-1, 1), between(-1, 1)};
        while (glm::dot(v, v) < 0.01f || glm::dot(v, v) > 1.0f);
        return glm::normalize(v);
    }

    struct Voxel
    {
        glm::vec3 p;
        std::uint8_t shade;
    };

    struct Asteroid
    {
        glm::vec2 pos, vel;
        int level = 0;
        Palette colour = cyan_c;
        glm::vec3 axis{0, 0, 1};
        float angle = 0, spin = 0;
        std::vector<Voxel> voxels;
        float radius() const { return level_radius[level] * 0.92f; }
    };

    // A lumpy voxel blob: a sphere with a few smooth bulges, a couple of craters bitten out, reduced to its shell.
    std::vector<Voxel> make_blob(int level)
    {
        const float R = level_radius[level], vs = level_voxel[level];
        const int n = static_cast<int>(std::ceil(R * 1.25f / vs));
        const int side = 2 * n + 1;
        struct Lobe
        {
            glm::vec3 dir;
            float freq, phase, amp;
        };
        Lobe lobes[5];
        for (Lobe &l : lobes)
            l = {random_unit(), between(1.5f, 3.5f), between(0.0f, 6.28f), between(0.05f, 0.11f)};
        struct Crater
        {
            glm::vec3 centre;
            float radius;
        };
        Crater craters[3];
        for (Crater &c : craters)
            c = {random_unit() * R * 0.95f, R * between(0.22f, 0.34f)};

        std::vector<char> solid(static_cast<std::size_t>(side * side * side), 0);
        auto index = [&](int i, int j, int k) { return static_cast<std::size_t>(((k + n) * side + (j + n)) * side + (i + n)); };
        for (int k = -n; k <= n; ++k)
            for (int j = -n; j <= n; ++j)
                for (int i = -n; i <= n; ++i)
                {
                    const glm::vec3 p = glm::vec3(i, j, k) * vs;
                    const float len = glm::length(p);
                    if (len < 1e-4f)
                    {
                        solid[index(i, j, k)] = 1;
                        continue;
                    }
                    const glm::vec3 d = p / len;
                    float bulge = 0;
                    for (const Lobe &l : lobes)
                        bulge += l.amp * std::sin(glm::dot(d, l.dir) * l.freq * 3.0f + l.phase);
                    const float limit = R * (0.88f + std::clamp(bulge, -0.28f, 0.3f));
                    bool in = glm::length(glm::vec3(p.x, p.y, p.z / 0.85f)) <= limit;
                    for (const Crater &c : craters)
                        if (glm::length(p - c.centre) < c.radius)
                            in = false;
                    solid[index(i, j, k)] = in;
                }
        std::vector<Voxel> out;
        auto filled = [&](int i, int j, int k) {
            return i >= -n && i <= n && j >= -n && j <= n && k >= -n && k <= n && solid[index(i, j, k)];
        };
        for (int k = -n; k <= n; ++k)
            for (int j = -n; j <= n; ++j)
                for (int i = -n; i <= n; ++i)
                    if (filled(i, j, k) && (!filled(i + 1, j, k) || !filled(i - 1, j, k) || !filled(i, j + 1, k) ||
                                            !filled(i, j - 1, k) || !filled(i, j, k + 1) || !filled(i, j, k - 1)))
                        out.push_back({glm::vec3(i, j, k) * vs, static_cast<std::uint8_t>(between(215.0f, 255.0f))});
        return out;
    }

    struct Dust
    {
        glm::vec3 pos, vel, axis;
        float angle, spin, age, life, size;
        Palette colour;
    };

    struct Glow
    {
        glm::vec3 pos, vel{0.0f};
        float age = 0, life = 0.5f, size0 = 1, size1 = 1, strength = 1;
        sl::Colour colour;
    };

    struct Bullet
    {
        glm::vec2 pos, vel;
        float life;
    };

    struct Ship
    {
        glm::vec2 pos{0.0f}, vel{0.0f};
        float heading = pi * 0.5f;
        bool thrusting = false;
        float bank = 0;
    };

    struct Sprite
    {
        int w, h;
        std::vector<std::string> rows;
    };
    // Nose up.
    const Sprite ship_sprite = {9, 10,
                                {"....X....", "....X....", "...XXX...", "...XXX...", "..XXXXX..", "..XX.XX..", ".XXX.XXX.", ".XX...XX.",
                                 "XXX...XXX", "XX.....XX"}};
    constexpr float ship_voxel = 0.65f;
    constexpr float ship_radius = 2.3f;

    std::vector<glm::vec3> ship_offsets()
    {
        std::vector<glm::vec3> out;
        for (int j = 0; j < ship_sprite.h; ++j)
            for (int i = 0; i < ship_sprite.w; ++i)
                if (ship_sprite.rows[j][i] == 'X')
                    out.push_back({(i - (ship_sprite.w - 1) * 0.5f) * ship_voxel, ((ship_sprite.h - 1) * 0.5f - j) * ship_voxel, 0.0f});
        return out;
    }

    struct Audio
    {
        sl::Sample *shoot = nullptr, *boom[3] = {nullptr, nullptr, nullptr}, *die = nullptr, *thrust = nullptr, *start = nullptr,
                   *extra = nullptr, *beat[2] = {nullptr, nullptr};
        bool ready = false;
    };

    sl::Sample *synth(float duration, sl::Adsr envelope, std::vector<sl::SynthOscillator> oscillators)
    {
        sl::SynthParams params;
        params.duration = duration;
        params.envelope = envelope;
        params.oscillators = std::move(oscillators);
        return sl::generate_sample(params);
    }

    void init_audio(Audio &a)
    {
        if (!sl::audio_fx_init())
            return;
        using W = sl::Waveform;
        a.shoot = synth(0.18f, {0.001f, 0.05f, 0.3f, 0.07f}, {{W::square, 1500.0f, 0.4f, {1.0f, 0.12f}}, {W::saw, 750.0f, 0.2f, {1.0f, 0.2f}}});
        a.boom[0] = synth(1.0f, {0.002f, 0.35f, 0.4f, 0.45f}, {{W::noise, 1.0f, 0.7f, {}}, {W::saw, 140.0f, 0.5f, {1.0f, 0.1f}}, {W::sine, 60.0f, 0.6f, {1.0f, 0.4f}}});
        a.boom[1] = synth(0.6f, {0.002f, 0.2f, 0.4f, 0.3f}, {{W::noise, 1.0f, 0.6f, {}}, {W::saw, 200.0f, 0.4f, {1.0f, 0.1f}}});
        a.boom[2] = synth(0.35f, {0.001f, 0.12f, 0.3f, 0.15f}, {{W::noise, 1.0f, 0.5f, {}}, {W::square, 500.0f, 0.3f, {1.0f, 0.1f}}});
        a.die = synth(1.4f, {0.002f, 0.5f, 0.4f, 0.6f}, {{W::noise, 1.0f, 0.8f, {}}, {W::saw, 120.0f, 0.6f, {1.0f, 0.05f}}, {W::sine, 50.0f, 0.7f, {1.0f, 0.3f}}});
        a.thrust = synth(0.1f, {0.005f, 0.03f, 0.6f, 0.04f}, {{W::noise, 1.0f, 0.35f, {}}, {W::sine, 70.0f, 0.4f, {}}});
        a.start = synth(0.5f, {0.01f, 0.1f, 0.5f, 0.2f}, {{W::square, 220.0f, 0.3f, {1.0f, 2.0f}}, {W::triangle, 440.0f, 0.3f, {1.0f, 2.0f}}});
        a.extra = synth(0.6f, {0.005f, 0.1f, 0.5f, 0.3f}, {{W::square, 660.0f, 0.3f, {1.0f, 2.0f}}, {W::triangle, 990.0f, 0.3f, {1.0f, 2.0f}}});
        a.beat[0] = synth(0.14f, {0.002f, 0.05f, 0.5f, 0.06f}, {{W::square, 55.0f, 0.5f, {}}, {W::sine, 27.5f, 0.6f, {}}});
        a.beat[1] = synth(0.14f, {0.002f, 0.05f, 0.5f, 0.06f}, {{W::square, 49.0f, 0.5f, {}}, {W::sine, 24.5f, 0.6f, {}}});
        a.ready = a.shoot && a.boom[2] && a.die && a.thrust && a.start && a.extra && a.beat[1];
    }

    void play(const Audio &a, sl::Sample *s, float x, int volume = 20, int frequency = 1000)
    {
        if (a.ready && s)
            sl::play_sample(s, volume, std::clamp(static_cast<int>((x + half_w) / dom_w * 255.0f), 0, 255), frequency);
    }

    enum class State { title, playing, player_dead, wave_clear, game_over };

    struct Game
    {
        State state = State::title;
        int score = 0, hiscore = 0, lives = 3, wave = 0, next_extra = 10000;
        Ship ship;
        float invuln = 0, state_timer = 0, shake = 0, fire_cooldown = 0, beat_timer = 0, thrust_blip = 0, time = 0;
        int beat = 0;
        std::string message;
        float message_time = 0;
        std::vector<Asteroid> asteroids;
        std::vector<Bullet> bullets;
        std::vector<Dust> dust;
        std::vector<Glow> glows;
        Audio audio;
    };

    void add_glow(Game &g, glm::vec3 pos, glm::vec3 vel, float life, float s0, float s1, sl::Colour colour, float strength = 1.0f)
    {
        Glow glow;
        glow.pos = pos;
        glow.vel = vel;
        glow.life = life;
        glow.size0 = s0;
        glow.size1 = s1;
        glow.colour = colour;
        glow.strength = strength;
        g.glows.push_back(glow);
    }

    void explosion_fx(Game &g, glm::vec3 at, sl::Colour colour, float size)
    {
        add_glow(g, at, {0, 0, 0}, 0.5f, size * 0.6f, size * 2.2f, {255, 255, 255}, 1.0f);
        add_glow(g, at, {0, 0, 0}, 0.85f, size * 0.8f, size * 3.2f, colour, 0.9f);
        for (int i = 0; i < 12; ++i)
        {
            const float a = uniform() * 6.2832f, s = between(8.0f, 28.0f);
            add_glow(g, at, {std::cos(a) * s, std::sin(a) * s, between(-4.0f, 8.0f)}, between(0.4f, 0.9f), 1.2f, 0.2f, colour, 1.0f);
        }
    }

    void spawn_dust(Game &g, glm::vec3 pos, glm::vec3 vel, Palette colour, float size)
    {
        if (g.dust.size() > 5000)
            return;
        Dust d;
        d.pos = pos;
        d.vel = vel;
        d.axis = random_unit();
        d.angle = between(0.0f, 6.28f);
        d.spin = between(-9.0f, 9.0f);
        d.age = 0;
        d.life = between(0.9f, 1.9f);
        d.size = size;
        d.colour = colour;
        g.dust.push_back(d);
    }

    glm::mat4 asteroid_matrix(const Asteroid &a, glm::vec2 at)
    {
        return glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(at, 0.0f)), a.angle, a.axis);
    }

    Asteroid make_asteroid(int level, glm::vec2 pos, glm::vec2 vel, Palette colour)
    {
        Asteroid a;
        a.level = level;
        a.pos = pos;
        a.vel = vel;
        a.colour = colour;
        a.axis = random_unit();
        a.angle = between(0.0f, 6.28f);
        a.spin = between(-1.2f, 1.2f) * (1.0f + level * 0.6f);
        a.voxels = make_blob(level);
        return a;
    }

    Palette random_rock_colour() { return static_cast<Palette>(std::rand() % 6); }

    // Split a rock: a shower of loose voxels, plus two or three smaller lumps unless it was already the smallest.
    void break_asteroid(Game &g, std::size_t index, glm::vec2 impact, bool award)
    {
        const Asteroid a = g.asteroids[index];
        g.asteroids.erase(g.asteroids.begin() + static_cast<std::ptrdiff_t>(index));
        const glm::mat4 m = asteroid_matrix(a, a.pos);
        const int want = level_dust[a.level];
        const float vs = level_voxel[a.level];
        for (int i = 0; i < want && !a.voxels.empty(); ++i)
        {
            const Voxel &v = a.voxels[static_cast<std::size_t>(std::rand()) % a.voxels.size()];
            const glm::vec3 world = glm::vec3(m * glm::vec4(v.p, 1.0f));
            glm::vec3 out = world - glm::vec3(a.pos, 0.0f);
            out = glm::length(out) > 1e-3f ? glm::normalize(out) : random_unit();
            const glm::vec3 vel = out * between(4.0f, 20.0f) + glm::vec3(a.vel * 0.5f + impact * 0.15f, between(-6.0f, 8.0f));
            spawn_dust(g, world, vel, a.colour, vs * between(0.6f, 1.0f));
        }
        explosion_fx(g, {a.pos, 0.5f}, palette[a.colour], level_radius[a.level] * 0.5f);
        play(g.audio, g.audio.boom[a.level], a.pos.x, a.level == 0 ? 255 : 210, a.level == 0 ? 900 : 1000 + a.level * 150);
        g.shake = std::max(g.shake, 0.08f + 0.06f * static_cast<float>(2 - a.level));
        if (award)
        {
            g.score += level_points[a.level];
            g.hiscore = std::max(g.hiscore, g.score);
            if (g.score >= g.next_extra)
            {
                g.next_extra += 10000;
                ++g.lives;
                g.message = "EXTRA SHIP";
                g.message_time = 2.0f;
                play(g.audio, g.audio.extra, 0.0f, 220);
            }
        }
        if (a.level >= 2)
            return;
        const int pieces = 2 + std::rand() % 2;
        const float base = uniform() * 6.2832f;
        const float child_r = level_radius[a.level + 1];
        for (int i = 0; i < pieces; ++i)
        {
            const float ang = base + i * 6.2832f / pieces + between(-0.3f, 0.3f);
            const glm::vec2 dir(std::cos(ang), std::sin(ang));
            const float speed = between(5.0f, 9.0f) + a.level * 5.0f;
            Asteroid c = make_asteroid(a.level + 1, wrap_pos(a.pos + dir * child_r * 0.8f), a.vel * 0.6f + dir * speed + impact * 0.1f,
                                       uniform() < 0.6f ? a.colour : random_rock_colour());
            g.asteroids.push_back(std::move(c));
        }
    }

    void start_wave(Game &g)
    {
        ++g.wave;
        const int count = std::min(3 + g.wave, 9);
        for (int i = 0; i < count; ++i)
        {
            glm::vec2 p;
            do
                p = {between(-half_w, half_w), between(-half_h, half_h)};
            while (glm::length(p - g.ship.pos) < 26.0f);
            const float a = uniform() * 6.2832f, s = between(5.0f, 8.0f + std::min(g.wave, 8) * 0.9f);
            g.asteroids.push_back(make_asteroid(0, p, {std::cos(a) * s, std::sin(a) * s}, random_rock_colour()));
        }
        g.beat_timer = 1.0f;
    }

    void new_game(Game &g)
    {
        g.score = 0;
        g.lives = 3;
        g.wave = 0;
        g.next_extra = 10000;
        g.ship = Ship();
        g.invuln = 2.5f;
        g.asteroids.clear();
        g.bullets.clear();
        g.dust.clear();
        g.glows.clear();
        start_wave(g);
        g.state = State::playing;
        play(g.audio, g.audio.start, 0.0f);
    }

    void kill_player(Game &g)
    {
        const glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(g.ship.pos, 0.0f)), g.ship.heading - pi * 0.5f, glm::vec3(0, 0, 1));
        for (const glm::vec3 &o : ship_offsets())
        {
            const glm::vec3 world = glm::vec3(m * glm::vec4(o, 1.0f));
            glm::vec3 out = world - glm::vec3(g.ship.pos, 0.0f);
            out = glm::length(out) > 1e-3f ? glm::normalize(out) : random_unit();
            spawn_dust(g, world, out * between(6.0f, 26.0f) + glm::vec3(g.ship.vel * 0.5f, between(-5.0f, 8.0f)), ship_c, ship_voxel);
        }
        explosion_fx(g, {g.ship.pos, 0.5f}, palette[ship_c], 6.0f);
        play(g.audio, g.audio.die, g.ship.pos.x, 255);
        g.shake = 0.9f;
        --g.lives;
        g.state = State::player_dead;
        g.state_timer = 2.2f;
    }

    struct Input
    {
        bool left = false, right = false, thrust = false, fire = false;
    };

    // A simple bot for --autoplay: shoot the nearest rock with some lead, and back away from one that gets too close.
    Input bot_input(const Game &g)
    {
        Input in;
        if (g.state != State::playing)
            return in;
        const Asteroid *nearest = nullptr;
        float best = 1e9f;
        for (const Asteroid &a : g.asteroids)
        {
            const float d = glm::length(wrap_delta(a.pos - g.ship.pos));
            if (d < best)
            {
                best = d;
                nearest = &a;
            }
        }
        if (!nearest)
            return in;
        const glm::vec2 rel = wrap_delta(nearest->pos - g.ship.pos);
        const glm::vec2 relv = nearest->vel - g.ship.vel;
        float t = best / 75.0f;
        for (int i = 0; i < 2; ++i)
            t = glm::length(rel + relv * t) / 75.0f;
        const glm::vec2 aim = rel + relv * t;
        const bool flee = best < nearest->radius() + 11.0f;
        const float want = std::atan2(aim.y, aim.x) + (flee ? pi : 0.0f);
        const float diff = wrap_delta(want - g.ship.heading, 2.0f * pi);
        in.left = diff > 0.06f;
        in.right = diff < -0.06f;
        if (flee)
            in.thrust = std::fabs(diff) < 0.5f && glm::length(g.ship.vel) < 28.0f;
        else
            in.fire = std::fabs(diff) < 0.1f && best < 60.0f;
        return in;
    }

    void update(Game &g, float dt, const Input &in)
    {
        g.time += dt;
        g.shake = std::max(0.0f, g.shake - dt);
        g.message_time = std::max(0.0f, g.message_time - dt);
        for (Glow &gl : g.glows)
        {
            gl.age += dt;
            gl.pos += gl.vel * dt;
            gl.vel *= std::max(0.0f, 1.0f - 2.0f * dt);
        }
        g.glows.erase(std::remove_if(g.glows.begin(), g.glows.end(), [](const Glow &gl) { return gl.age >= gl.life; }), g.glows.end());
        for (Dust &d : g.dust)
        {
            d.age += dt;
            d.pos += d.vel * dt;
            d.vel *= std::max(0.0f, 1.0f - 1.1f * dt);
            d.angle += d.spin * dt;
        }
        g.dust.erase(std::remove_if(g.dust.begin(), g.dust.end(), [](const Dust &d) { return d.age >= d.life; }), g.dust.end());
        for (Asteroid &a : g.asteroids)
        {
            a.pos = wrap_pos(a.pos + a.vel * dt);
            a.angle += a.spin * dt;
        }

        if (g.state == State::title || g.state == State::game_over)
            return;

        if (g.state == State::player_dead)
        {
            g.state_timer -= dt;
            if (g.state_timer <= 0)
            {
                if (g.lives <= 0)
                {
                    g.state = State::game_over;
                    g.message = "";
                }
                else
                {
                    bool clear = true;
                    for (const Asteroid &a : g.asteroids)
                        clear &= glm::length(wrap_delta(a.pos)) > a.radius() + 14.0f;
                    if (clear)
                    {
                        g.ship = Ship();
                        g.invuln = 2.5f;
                        g.state = State::playing;
                    }
                }
            }
        }
        else if (g.state == State::wave_clear)
        {
            g.state_timer -= dt;
            if (g.state_timer <= 0)
            {
                start_wave(g);
                g.state = State::playing;
                g.message = "WAVE " + std::to_string(g.wave);
                g.message_time = 2.0f;
            }
        }

        // Heartbeat that speeds up as the rocks thin out.
        g.beat_timer -= dt;
        if (g.beat_timer <= 0 && !g.asteroids.empty())
        {
            play(g.audio, g.audio.beat[g.beat], 0.0f, 90);
            g.beat ^= 1;
            g.beat_timer = std::clamp(0.28f + static_cast<float>(g.asteroids.size()) * 0.06f, 0.3f, 1.0f);
        }

        if (g.state == State::playing)
        {
            Ship &s = g.ship;
            g.invuln = std::max(0.0f, g.invuln - dt);
            g.fire_cooldown = std::max(0.0f, g.fire_cooldown - dt);
            if (in.left)
                s.heading += 4.4f * dt;
            if (in.right)
                s.heading -= 4.4f * dt;
            const glm::vec2 fwd(std::cos(s.heading), std::sin(s.heading));
            const float turn = (in.left ? 1.0f : 0.0f) - (in.right ? 1.0f : 0.0f);
            s.bank += (turn * 0.7f - s.bank) * std::min(1.0f, 8.0f * dt);
            s.thrusting = in.thrust;
            if (in.thrust)
            {
                s.vel += fwd * 46.0f * dt;
                const float speed = glm::length(s.vel);
                if (speed > 58.0f)
                    s.vel *= 58.0f / speed;
                const glm::vec2 tail = s.pos - fwd * 3.2f;
                add_glow(g, {tail, 0.0f}, {-fwd * 22.0f + glm::vec2(between(-3, 3), between(-3, 3)), 0.0f}, 0.35f, 1.8f, 0.3f, {255, 120, 20}, 0.9f);
                add_glow(g, {tail, 0.1f}, {-fwd * 14.0f, 0.0f}, 0.22f, 1.2f, 0.2f, {120, 230, 255}, 1.0f);
                g.thrust_blip -= dt;
                if (g.thrust_blip <= 0)
                {
                    play(g.audio, g.audio.thrust, s.pos.x, 70);
                    g.thrust_blip = 0.11f;
                }
            }
            s.vel *= std::max(0.0f, 1.0f - 0.3f * dt);
            s.pos = wrap_pos(s.pos + s.vel * dt);

            if (in.fire && g.fire_cooldown <= 0 && g.bullets.size() < 6)
            {
                g.bullets.push_back({wrap_pos(s.pos + fwd * 3.4f), fwd * 78.0f + s.vel * 0.4f, 0.95f});
                g.fire_cooldown = 0.17f;
                play(g.audio, g.audio.shoot, s.pos.x, 190);
            }
        }

        // Bullets, moved in small steps so none tunnel through a lump.
        for (Bullet &b : g.bullets)
        {
            b.life -= dt;
            const int steps = std::max(1, static_cast<int>(std::ceil(glm::length(b.vel) * dt / 1.0f)));
            for (int k = 0; k < steps && b.life > 0; ++k)
            {
                b.pos = wrap_pos(b.pos + b.vel * dt / static_cast<float>(steps));
                for (std::size_t i = 0; i < g.asteroids.size(); ++i)
                    if (glm::length(wrap_delta(g.asteroids[i].pos - b.pos)) < g.asteroids[i].radius())
                    {
                        break_asteroid(g, i, b.vel, true);
                        b.life = 0;
                        break;
                    }
            }
            if (b.life > 0)
                add_glow(g, {b.pos, 0.0f}, {0, 0, 0}, 0.25f, 1.1f, 0.2f, {255, 80, 220}, 0.7f);
        }
        g.bullets.erase(std::remove_if(g.bullets.begin(), g.bullets.end(), [](const Bullet &b) { return b.life <= 0; }), g.bullets.end());

        if (g.state == State::playing && g.invuln <= 0)
            for (std::size_t i = 0; i < g.asteroids.size(); ++i)
                if (glm::length(wrap_delta(g.asteroids[i].pos - g.ship.pos)) < g.asteroids[i].radius() + ship_radius)
                {
                    break_asteroid(g, i, g.asteroids[i].vel, true);
                    kill_player(g);
                    break;
                }

        if (g.state == State::playing && g.asteroids.empty())
        {
            g.state = State::wave_clear;
            g.state_timer = 2.4f;
        }
    }

    // --- rendering ---------------------------------------------------------------------------------------------

    struct Renderer
    {
        sl::Surface3D *surface = nullptr;
        sl::Mesh *cube = nullptr, *quad = nullptr;
        sl::Bitmap *puff = nullptr;
        sl::Bloom bloom;
        struct Star
        {
            glm::vec3 pos;
            float size, phase;
            sl::Colour colour;
        };
        std::vector<Star> stars;
        std::vector<glm::vec3> grid_pos, grid_size;
    };

    sl::Mesh *make_quad()
    {
        std::vector<sl::Vertex3D> vertices(4);
        const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (int i = 0; i < 4; ++i)
        {
            vertices[i].position[0] = corner[i][0];
            vertices[i].position[1] = corner[i][1];
            vertices[i].position[2] = 0.0f;
            vertices[i].normal[0] = 0.0f;
            vertices[i].normal[1] = 0.0f;
            vertices[i].normal[2] = 1.0f;
            vertices[i].uv[0] = 0.5f + 0.5f * corner[i][0];
            vertices[i].uv[1] = 0.5f - 0.5f * corner[i][1];
        }
        return sl::create_mesh(vertices, {0, 1, 2, 0, 2, 3});
    }

    sl::Bitmap *make_puff()
    {
        constexpr int n = 64;
        sl::Bitmap *bitmap = sl::create_bitmap(n, n);
        if (!bitmap)
            return nullptr;
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                const double dx = (x + 0.5) / n * 2.0 - 1.0, dy = (y + 0.5) / n * 2.0 - 1.0;
                const double a = std::clamp(1.0 - std::sqrt(dx * dx + dy * dy), 0.0, 1.0);
                sl::putpixel(bitmap, x, y, {255, 255, 255, static_cast<Uint8>(255.0 * a * a * (3.0 - 2.0 * a))});
            }
        bitmap->smooth = true;
        return bitmap;
    }

    void build_scenery(Renderer &r)
    {
        for (int i = 0; i < 300; ++i)
        {
            Renderer::Star s;
            s.pos = {between(-170.0f, 170.0f), between(-130.0f, 130.0f), between(-180.0f, -50.0f)};
            s.size = between(0.15f, 0.5f);
            s.phase = between(0.0f, 6.28f);
            const float t = uniform();
            s.colour = t < 0.5f ? sl::Colour{200, 220, 255} : (t < 0.75f ? sl::Colour{255, 120, 240} : sl::Colour{90, 255, 255});
            r.stars.push_back(s);
        }
        // A receding neon floor grid under the field, so the tilted camera shows real depth.
        for (float x = -150.0f; x <= 150.0f; x += 12.0f)
        {
            r.grid_pos.push_back({x, -half_h - 8.0f, -80.0f});
            r.grid_size.push_back({0.16f, 0.1f, 200.0f});
        }
        for (float z = 40.0f; z >= -200.0f; z -= 16.0f)
        {
            r.grid_pos.push_back({0.0f, -half_h - 8.0f, z});
            r.grid_size.push_back({300.0f, 0.1f, 0.16f});
        }
        // A neon wall grid far behind the action.
        for (float x = -150.0f; x <= 150.0f; x += 12.0f)
        {
            r.grid_pos.push_back({x, 0.0f, -70.0f});
            r.grid_size.push_back({0.16f, 200.0f, 0.1f});
        }
        for (float y = -96.0f; y <= 96.0f; y += 12.0f)
        {
            r.grid_pos.push_back({0.0f, y, -70.0f});
            r.grid_size.push_back({300.0f, 0.16f, 0.1f});
        }
    }

    void draw_voxel_box(std::vector<sl::InstanceData3D> &out, glm::vec3 centre, glm::vec3 size, sl::Colour tint = {255, 255, 255, 255})
    {
        sl::InstanceData3D inst;
        inst.model = glm::scale(glm::translate(glm::mat4(1.0f), centre), size);
        inst.tint = tint;
        out.push_back(inst);
    }

    void draw_glow(Renderer &r, glm::vec3 pos, glm::vec2 size, sl::Colour colour, float alpha, float rotation = 0.0f)
    {
        sl::Material m;
        m.lit = false;
        m.fog = false;
        m.blend = sl::MaterialBlend::additive;
        m.cull = sl::CullMode::none;
        m.texture = r.puff;
        m.tint = colour;
        m.tint.alpha = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
        model = glm::rotate(model, rotation, glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));
        sl::draw_mesh(r.quad, model, m);
    }

    // The field wraps, so anything near an edge is drawn again on the opposite side.
    template <class F> void for_wrapped(glm::vec2 pos, float radius, F &&draw)
    {
        for (int oy = -1; oy <= 1; ++oy)
            for (int ox = -1; ox <= 1; ++ox)
            {
                const glm::vec2 p = pos + glm::vec2(ox * dom_w, oy * dom_h);
                if (std::fabs(p.x) <= half_w + radius + 1.0f && std::fabs(p.y) <= half_h + radius + 1.0f)
                    draw(p);
            }
    }

    void draw_scene(Renderer &r, Game &g)
    {
        const float shake = g.shake > 0 ? g.shake * 1.2f : 0.0f;
        sl::Camera camera;
        camera.position = {std::sin(g.time * 0.23f) * 12.0f + between(-shake, shake), -24.0f + std::sin(g.time * 0.17f) * 4.0f + between(-shake, shake), 118.0f};
        camera.target = {0.0f, 2.0f, 0.0f};
        camera.field_of_view = 40.0f;
        camera.near_plane = 1.0f;
        camera.far_plane = 500.0f;
        if (!sl::begin_surface_3d(r.surface, camera))
            return;
        sl::clear_surface_3d({3, 2, 14});
        sl::Light3D light;
        light.direction = {-0.45f, -0.6f, -0.65f};
        light.ambient = 0.3f;
        sl::set_light_3d(light);

        {
            std::vector<sl::LocalLight3D> lights;
            for (const Bullet &b : g.bullets)
            {
                sl::LocalLight3D l;
                l.position = {b.pos, 6.0f};
                l.colour = {1.0f, 0.3f, 0.9f};
                l.intensity = 2.4f;
                l.range = 20.0f;
                lights.push_back(l);
            }
            for (const Glow &gl : g.glows)
                if (gl.life >= 0.5f && gl.size1 > 3.0f && lights.size() < sl::max_local_lights_3d)
                {
                    sl::LocalLight3D l;
                    l.position = gl.pos + glm::vec3(0, 0, 6.0f);
                    l.colour = {gl.colour.red / 255.0f, gl.colour.green / 255.0f, gl.colour.blue / 255.0f};
                    l.intensity = 3.2f * (1.0f - gl.age / gl.life);
                    l.range = 32.0f;
                    lights.push_back(l);
                }
            if (lights.size() > sl::max_local_lights_3d)
                lights.resize(sl::max_local_lights_3d);
            sl::set_local_lights_3d(lights);
        }

        {
            sl::Material m;
            m.lit = false;
            m.fog = false;
            m.tint = {70, 30, 200};
            std::vector<sl::InstanceData3D> lines;
            for (std::size_t i = 0; i < r.grid_pos.size(); ++i)
                draw_voxel_box(lines, r.grid_pos[i], r.grid_size[i]);
            sl::draw_mesh_instanced(r.cube, lines, m);
        }

        std::array<std::vector<sl::InstanceData3D>, palette_count> solid;
        for (const Asteroid &a : g.asteroids)
        {
            const float vs = level_voxel[a.level] * 0.96f;
            for_wrapped(a.pos, level_radius[a.level], [&](glm::vec2 p) {
                const glm::mat4 base = asteroid_matrix(a, p);
                for (const Voxel &v : a.voxels)
                {
                    sl::InstanceData3D inst;
                    inst.model = glm::scale(glm::translate(base, v.p), glm::vec3(vs));
                    inst.tint = {v.shade, v.shade, v.shade, 255};
                    solid[a.colour].push_back(inst);
                }
            });
        }
        const bool ship_blink = g.invuln > 0 && static_cast<int>(g.invuln * 10.0f) % 2 == 1;
        const bool ship_visible = g.state == State::playing && !ship_blink;
        const std::vector<glm::vec3> offsets = ship_offsets();
        if (ship_visible)
            for_wrapped(g.ship.pos, 3.0f, [&](glm::vec2 p) {
                glm::mat4 base = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(p, 0.0f)), g.ship.heading - pi * 0.5f, glm::vec3(0, 0, 1));
                base = glm::rotate(base, g.ship.bank, glm::vec3(0, 1, 0));
                for (const glm::vec3 &o : offsets)
                {
                    sl::InstanceData3D inst;
                    inst.model = glm::scale(glm::translate(base, o), glm::vec3(ship_voxel * 0.94f, ship_voxel * 0.94f, ship_voxel * 3.4f));
                    solid[ship_c].push_back(inst);
                }
            });
        // Spare lives, drawn as little ships.
        for (int i = 0; i < std::max(g.lives - (g.state == State::playing ? 1 : 0), 0); ++i)
            for (const glm::vec3 &o : offsets)
                draw_voxel_box(solid[ship_c], glm::vec3(-half_w + 8.0f + i * 4.2f, -half_h + 14.0f, 0.0f) + o * 0.7f,
                               glm::vec3(ship_voxel * 0.7f * 0.94f, ship_voxel * 0.7f * 0.94f, ship_voxel * 2.4f));

        auto material_for = [&](int colour, float emissive) {
            sl::Material m;
            m.lit = true;
            m.fog = false;
            m.tint = palette[colour];
            m.emissive[0] = palette[colour].red / 255.0f * emissive;
            m.emissive[1] = palette[colour].green / 255.0f * emissive;
            m.emissive[2] = palette[colour].blue / 255.0f * emissive;
            return m;
        };
        for (int c = 0; c < palette_count; ++c)
            if (!solid[c].empty())
                sl::draw_mesh_instanced(r.cube, solid[c], material_for(c, 1.3f));

        // The voxel shower fades by shrinking and becoming transparent.
        std::array<std::vector<sl::InstanceData3D>, palette_count> fading;
        for (const Dust &d : g.dust)
        {
            const float f = std::clamp((1.0f - d.age / d.life) * 1.8f, 0.0f, 1.0f);
            sl::InstanceData3D inst;
            inst.model = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), d.pos), d.angle, d.axis), glm::vec3(d.size * (0.35f + 0.65f * f)));
            inst.tint = {255, 255, 255, static_cast<std::uint8_t>(255.0f * f)};
            fading[d.colour].push_back(inst);
        }
        for (int c = 0; c < palette_count; ++c)
            if (!fading[c].empty())
            {
                sl::Material m = material_for(c, 1.5f);
                m.blend = sl::MaterialBlend::alpha;
                sl::draw_mesh_instanced(r.cube, fading[c], m);
            }

        // Additive glows, back to front: stars, rock halos, then ship, shots and explosions.
        for (const Renderer::Star &s : r.stars)
            draw_glow(r, s.pos, {s.size * 2.0f, s.size * 2.0f}, s.colour, 0.5f + 0.5f * std::sin(g.time * 1.5f + s.phase));
        for (const Asteroid &a : g.asteroids)
            for_wrapped(a.pos, level_radius[a.level], [&](glm::vec2 p) {
                const float s = level_radius[a.level] * 2.7f;
                draw_glow(r, {p, -level_radius[a.level] - 1.0f}, {s, s}, palette[a.colour], 0.38f);
            });
        if (ship_visible)
            for_wrapped(g.ship.pos, 4.0f, [&](glm::vec2 p) { draw_glow(r, {p, -1.0f}, {6.5f, 6.5f}, palette[ship_c], 0.4f); });
        for (const Bullet &b : g.bullets)
        {
            const float rot = std::atan2(b.vel.y, b.vel.x) - pi * 0.5f;
            draw_glow(r, {b.pos, 0.3f}, {1.1f, 3.8f}, {255, 60, 230}, 0.95f, rot);
            draw_glow(r, {b.pos, 0.4f}, {0.45f, 2.4f}, {255, 255, 255}, 1.0f, rot);
            draw_glow(r, {b.pos, 0.3f}, {3.0f, 3.0f}, {255, 120, 240}, 0.55f);
        }
        for (const Glow &gl : g.glows)
        {
            const float t = gl.age / gl.life;
            const float size = gl.size0 + (gl.size1 - gl.size0) * t;
            draw_glow(r, gl.pos, {size, size}, gl.colour, gl.strength * (1.0f - t) * (1.0f - t));
        }
        sl::end_surface_3d();
        r.bloom.apply(sl::surface_3d_bitmap(r.surface), 0, 0, window_w, window_h);
    }

    void draw_hud(Game &g)
    {
        const sl::Colour white{235, 240, 255}, cyan{80, 240, 255}, pink{255, 90, 230}, lime{170, 255, 60};
        sl::gprintf(24, 10, pink, "SCORE");
        sl::gprintf(24, 30, white, "%06d", g.score);
        sl::gprintf_center(10, pink, "HI-SCORE");
        sl::gprintf_center(30, white, "%06d", g.hiscore);
        sl::gprintf(window_w - 150, 10, pink, "WAVE");
        sl::gprintf(window_w - 150, 30, white, "%d", std::max(g.wave, 1));
        if (g.message_time > 0 && !g.message.empty())
            sl::gprintf_center(300, lime, "%s", g.message.c_str());
        if (g.state == State::title)
        {
            sl::gprintf_center(220, cyan, "E X A S T E R O I D S");
            sl::gprintf_center(260, white, "Press ENTER or SPACE to play");
            sl::gprintf_center(284, white, "Left / Right turn, Up thrusts, Space fires");
        }
        else if (g.state == State::game_over)
        {
            sl::gprintf_center(250, pink, "G A M E   O V E R");
            sl::gprintf_center(280, white, "Press ENTER to play again");
        }
        else if (g.state == State::wave_clear)
            sl::gprintf_center(280, lime, "WAVE CLEARED");
    }
} // namespace

int main(int argc, char *argv[])
{
    bool autoplay = false;
    std::string shot_path;
    float shot_at = 10.0f;
    std::vector<char *> backend_args{argv[0]};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--autoplay"))
            autoplay = true;
        else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc)
            shot_path = argv[++i];
        else if (!std::strcmp(argv[i], "--shot-at") && i + 1 < argc)
            shot_at = static_cast<float>(std::atof(argv[++i]));
        else
            backend_args.push_back(argv[i]);
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(backend_args.size()), backend_args.data()) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, window_w, window_h))
        return -1;
    sl::set_window_title("exasteroids v" EXASTEROIDS_VERSION);

    Renderer r;
    r.surface = sl::create_surface_3d(window_w, window_h, 4);
    r.cube = sl::create_cube_mesh(1.0f);
    r.quad = make_quad();
    r.puff = make_puff();
    if (!r.surface || !r.cube || !r.quad || !r.puff || !r.bloom.initialise())
    {
        std::fprintf(stderr, "exasteroids needs 3D surfaces and bloom, which this backend doesn't provide.\n");
        sl::shutdown();
        return -1;
    }
    r.bloom.set_threshold(0.4f);
    r.bloom.set_intensity(1.7f);
    r.bloom.set_radius(3.5f);
    build_scenery(r);

    static Game g;
    std::srand(11);
    init_audio(g.audio);
    start_wave(g); // the rocks drifting behind the title screen
    if (autoplay)
        new_game(g);

    sl::set_fps(60);
    bool running = true, saved = false;
    float last = static_cast<float>(sl::time_ms()) * 0.001f, elapsed = 0.0f;
    int game_over_frames = 0;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit || (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down &&
                     (event.key() == sl::Event::Key::return_key || event.key() == sl::Event::Key::keypad_enter ||
                      (event.key() == sl::Event::Key::space && g.state == State::title)))
            {
                if (g.state == State::title || g.state == State::game_over)
                    new_game(g);
            }
            sl::display_handle_event(event);
        }
        const float now = static_cast<float>(sl::time_ms()) * 0.001f;
        const float dt = std::min(now - last, 0.05f);
        last = now;
        elapsed += dt;

        Input in;
        if (autoplay)
        {
            game_over_frames = g.state == State::game_over ? game_over_frames + 1 : 0;
            if (game_over_frames > 120)
                new_game(g);
            in = bot_input(g);
        }
        else
        {
            in.left = sl::key_down(sl::Event::Key::arrow_left);
            in.right = sl::key_down(sl::Event::Key::arrow_right);
            in.thrust = sl::key_down(sl::Event::Key::arrow_up);
            in.fire = sl::key_down(sl::Event::Key::space);
        }
        update(g, dt, in);

        draw_scene(r, g);
        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(r.surface), 0, 0);
        draw_hud(g);
        if (!shot_path.empty() && !saved && elapsed >= shot_at)
        {
            sl::save_bitmap(sl::screen, shot_path);
            saved = true;
            running = false;
        }
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    if (g.audio.ready)
        sl::audio_fx_shutdown();
    r.bloom.shutdown();
    sl::destroy_bitmap(r.puff);
    sl::destroy_mesh(r.quad);
    sl::destroy_mesh(r.cube);
    sl::destroy_surface_3d(r.surface);
    sl::shutdown();
    return 0;
}
