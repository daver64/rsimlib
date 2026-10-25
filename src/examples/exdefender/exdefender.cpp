// exdefender: a neon-voxel tribute to the classic side-scrolling space-rescue arcade game.
// Move through a wraparound world, rescue stranded people and break up alien waves.
//
// Arrow keys move in four directions, Space fires and Z uses a smart bomb.
// Enter starts or restarts; Escape quits. --autoplay runs a demo, --shot FILE and
// --shot-at SECONDS save a screenshot and exit.
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
    constexpr float world_w = 240.0f, world_h = 82.0f;
    constexpr float voxel = 0.48f, floor_y = 2.0f;
    constexpr float ship_half_width = 15 * voxel * 1.2f * 0.5f;
    constexpr int human_count = 12, enemy_count = 8;
    constexpr float pi = 3.14159265f;

    enum Colour { ship_c, enemy_c, bomber_c, human_c, shot_c, ground_c, colour_count };
    const sl::Colour colours[colour_count] = {{70, 235, 255}, {255, 70, 205}, {255, 135, 40},
                                               {180, 255, 90}, {100, 220, 255}, {70, 120, 255}};
    // Modelled on the arcade ship sprite: nose to the right, G grey, M magenta, B blue, O orange,
    // R red, W white, N green.
    constexpr float ship_scale = 1.2f;
    const std::vector<std::string> ship_shape = {
        "...GG..........", "..GGGG.........", "R.GGGGG........", "..MMGGGGBBO....",
        "R.MMMGGGGGGGGWN", "...MMGWNG......"};
    const std::vector<std::string> fighter_shape = {
        "....XX....", "..XXXXXX..", ".XXXXXXXX.", "XXXXXXXXXX", "XX.XXXX.XX", "...X..X..."};
    const std::vector<std::string> bomber_shape = {
        "...XXXX...", ".XXXXXXXX.", "XXXXXXXXXX", "XX.XXXX.XX", ".XX.XX.XX.", "...X..X..."};
    const std::vector<std::string> human_shape = {"..X..", ".XXX.", "..X..", ".X.X.", "X...X"};

    float randf() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
    float between(float a, float b) { return a + (b - a) * randf(); }
    float wrap_x(float x)
    {
        while (x < -world_w * 0.5f)
            x += world_w;
        while (x >= world_w * 0.5f)
            x -= world_w;
        return x;
    }
    float near_x(float x, float origin)
    {
        float d = wrap_x(x - origin);
        return origin + d;
    }
    float distance_x(float a, float b) { return std::fabs(wrap_x(a - b)); }

    struct Glow
    {
        glm::vec3 pos, vel{0.0f};
        float age = 0, life = 0.5f, size0 = 1, size1 = 1, strength = 1;
        sl::Colour colour;
    };
    struct Shot
    {
        float x = 0, y = 0, vx = 0, vy = 0, life = 0;
        bool enemy = false;
    };
    struct Human
    {
        float x = 0, y = floor_y;
        bool alive = true, falling = false, abducted = false;
        int carrier = -1;
    };
    struct Enemy
    {
        float x = 0, y = 0, phase = 0;
        bool alive = false, bomber = false;
    };
    struct Audio
    {
        sl::Sample *shot = nullptr, *blast = nullptr, *rescue = nullptr, *start = nullptr;
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
        a.shot = synth(0.14f, {0.001f, 0.03f, 0.2f, 0.08f}, {{W::square, 1250, 0.45f, {1, 0.15f}}});
        a.blast = synth(0.8f, {0.002f, 0.3f, 0.3f, 0.3f}, {{W::noise, 1, 0.65f, {}}, {W::saw, 95, 0.4f, {1, 0.15f}}});
        a.rescue = synth(0.35f, {0.005f, 0.1f, 0.6f, 0.2f}, {{W::sine, 880, 0.5f, {1, 1.5f}}, {W::triangle, 1320, 0.25f, {1, 1.5f}}});
        a.start = synth(0.5f, {0.01f, 0.12f, 0.5f, 0.2f}, {{W::square, 330, 0.3f, {1, 2}}, {W::triangle, 660, 0.3f, {1, 2}}});
        a.ready = a.shot && a.blast && a.rescue && a.start;
    }
    void play(const Audio &a, sl::Sample *sample, float x, int volume = 180)
    {
        if (a.ready && sample)
            sl::play_sample(sample, volume, std::clamp(static_cast<int>((wrap_x(x) + world_w * 0.5f) / world_w * 255.0f), 0, 255), 1000);
    }

    enum class State { title, playing, dying, wave_clear, game_over };
    struct Game
    {
        State state = State::title;
        int score = 0, hiscore = 0, lives = 3, wave = 0, rescued = 0, bombs = 2;
        float x = 0, y = 35, vx = 0, vy = 0, invulnerable = 0;
        float timer = 0, fire_timer = 0, enemy_timer = 0, smart_flash = 0, time = 0;
        bool facing_left = false;
        Human humans[human_count];
        Enemy enemies[enemy_count];
        std::vector<Shot> shots;
        std::vector<Glow> glows;
        sl::RigidBoxWorld3D debris;
        Audio audio;
    };

    struct Input
    {
        bool left = false, right = false, up = false, down = false, fire = false, bomb = false;
    };

    void add_glow(Game &g, glm::vec3 pos, glm::vec3 vel, float life, float s0, float s1, sl::Colour colour, float strength = 1)
    {
        g.glows.push_back({pos, vel, 0, life, s0, s1, strength, colour});
    }
    void explosion(Game &g, float x, float y, Colour colour, float size)
    {
        const glm::vec3 at{x, y, 0.5f};
        add_glow(g, at, {}, 0.5f, size * 0.5f, size * 2.2f, {255, 255, 255});
        add_glow(g, at, {}, 0.8f, size * 0.6f, size * 3.0f, colours[colour], 0.9f);
        for (int i = 0; i < 12; ++i)
        {
            const float a = randf() * 2 * pi, speed = between(5, 18);
            add_glow(g, at, {std::cos(a) * speed, std::sin(a) * speed, between(-3, 7)}, between(0.35f, 0.8f), 0.8f, 0.12f,
                     colours[colour]);
        }
    }
    void shatter(Game &g, float x, float y, Colour colour, int count = 18)
    {
        sl::RigidBoxWorld3D burst;
        for (int i = 0; i < count; ++i)
        {
            sl::RigidBox3D body;
            body.position = {x + between(-2.5f, 2.5f), y + between(-1.8f, 1.8f), between(-0.3f, 0.3f)};
            body.half_extents = {voxel * 0.42f, voxel * 0.42f, voxel * 1.4f};
            body.lifetime = between(1.0f, 2.1f);
            body.restitution = 0.45f;
            body.friction = 0.35f;
            body.user = static_cast<std::uint32_t>(colour);
            burst.add(body);
        }
        burst.explode({x, y, -1}, 8, 24, 7);
        for (sl::RigidBox3D &body : burst.bodies())
            g.debris.bodies().push_back(body);
    }
    void reset_wave(Game &g)
    {
        ++g.wave;
        g.shots.clear();
        for (int i = 0; i < human_count; ++i)
            g.humans[i] = {wrap_x(-world_w * 0.5f + 12 + i * (world_w - 24) / (human_count - 1)), floor_y, true, false,
                           false, -1};
        for (int i = 0; i < enemy_count; ++i)
            g.enemies[i] = {wrap_x(-world_w * 0.5f + 15 + i * 29.0f), between(25, 70), between(0, 2 * pi), true, (i % 3) == 0};
        g.enemy_timer = 1.3f;
    }
    // Puts the ship somewhere with no enemy or enemy shot nearby, preferring the spot it already occupies.
    void place_player_safely(Game &g, float preferred_x, float preferred_y)
    {
        constexpr float enemy_clearance = 32.0f, shot_clearance = 22.0f;
        auto clearance = [&](float x, float y) {
            float worst = 1e9f;
            for (const Enemy &e : g.enemies)
                if (e.alive)
                    worst = std::min(worst, std::hypot(distance_x(e.x, x), (e.y - y) * 0.6f) / enemy_clearance);
            for (const Shot &s : g.shots)
                if (s.enemy)
                    worst = std::min(worst, std::hypot(distance_x(s.x, x), s.y - y) / shot_clearance);
            return worst;
        };
        float best_x = preferred_x, best_y = preferred_y, best = clearance(preferred_x, preferred_y);
        for (int i = 0; i < 96 && best < 1.0f; ++i)
        {
            const float x = i == 0 ? preferred_x : wrap_x(between(-world_w * 0.5f, world_w * 0.5f));
            const float y = between(floor_y + 8.0f, world_h - 8.0f);
            const float c = clearance(x, y);
            if (c > best)
                best = c, best_x = x, best_y = y;
        }
        g.x = best_x;
        g.y = best_y;
        g.vx = g.vy = 0;
    }
    void new_game(Game &g)
    {
        g.score = 0;
        g.lives = 3;
        g.rescued = 0;
        g.bombs = 2;
        g.wave = 0;
        g.x = 0;
        g.y = 36;
        g.facing_left = false;
        g.invulnerable = 1.5f;
        g.debris.clear();
        g.glows.clear();
        g.vx = g.vy = 0;
        reset_wave(g);
        place_player_safely(g, g.x, g.y);
        g.state = State::playing;
        play(g.audio, g.audio.start, 0);
    }
    void kill_player(Game &g)
    {
        shatter(g, g.x, g.y, ship_c, 28);
        explosion(g, g.x, g.y, ship_c, 5);
        play(g.audio, g.audio.blast, g.x, 255);
        g.shots.erase(std::remove_if(g.shots.begin(), g.shots.end(), [](const Shot &s) { return !s.enemy; }), g.shots.end());
        --g.lives;
        g.state = State::dying;
        g.timer = 1.8f;
        g.invulnerable = 2.2f;
    }
    void destroy_enemy(Game &g, Enemy &e)
    {
        const int enemy_index = static_cast<int>(&e - g.enemies);
        e.alive = false;
        for (Human &h : g.humans)
            if (h.alive && h.abducted && h.carrier == enemy_index)
            {
                h.abducted = false;
                h.falling = true;
                h.carrier = -1;
                h.y = e.y - 2.5f;
            }
        shatter(g, e.x, e.y, e.bomber ? bomber_c : enemy_c);
        explosion(g, e.x, e.y, e.bomber ? bomber_c : enemy_c, e.bomber ? 3.4f : 2.4f);
        g.score += e.bomber ? 150 : 100;
        g.hiscore = std::max(g.hiscore, g.score);
        play(g.audio, g.audio.blast, e.x, 200);
    }
    void rescue_human(Game &g, Human &h)
    {
        h.alive = false;
        ++g.rescued;
        g.score += 500;
        g.hiscore = std::max(g.hiscore, g.score);
        add_glow(g, {h.x, h.y, 0.5f}, {0, 18, 0}, 0.6f, 1.5f, 4, colours[human_c]);
        play(g.audio, g.audio.rescue, h.x, 210);
    }

    void update(Game &g, float dt, const Input &in)
    {
        g.time += dt;
        g.invulnerable = std::max(0.0f, g.invulnerable - dt);
        g.fire_timer = std::max(0.0f, g.fire_timer - dt);
        g.smart_flash = std::max(0.0f, g.smart_flash - dt);
        g.debris.step(dt, 2);
        for (Glow &gl : g.glows)
        {
            gl.age += dt;
            gl.pos += gl.vel * dt;
            gl.vel *= std::max(0.0f, 1.0f - 1.5f * dt);
        }
        g.glows.erase(std::remove_if(g.glows.begin(), g.glows.end(), [](const Glow &gl) { return gl.age >= gl.life; }), g.glows.end());
        if (g.state == State::title || g.state == State::game_over)
            return;
        if (g.state == State::dying || g.state == State::wave_clear)
        {
            g.timer -= dt;
            if (g.timer <= 0)
            {
                if (g.state == State::dying)
                {
                    if (g.lives <= 0)
                        g.state = State::game_over;
                    else
                    {
                        g.state = State::playing;
                        place_player_safely(g, 0, 35);
                        g.facing_left = false;
                    }
                }
                else
                {
                    reset_wave(g);
                    place_player_safely(g, g.x, g.y);
                    g.invulnerable = std::max(g.invulnerable, 1.5f);
                    g.state = State::playing;
                }
            }
            return;
        }

        const float move_x = static_cast<float>(in.right) - static_cast<float>(in.left);
        const float move_y = static_cast<float>(in.up) - static_cast<float>(in.down);
        if (move_x != 0)
            g.facing_left = move_x < 0;
        // Momentum: input accelerates the ship, drag bleeds speed off, and the speed is clamped.
        constexpr float max_speed_x = 40.0f, max_speed_y = 28.0f, accel = 200.0f, drag = 4.0f;
        g.vx += move_x * accel * dt;
        g.vy += move_y * accel * dt;
        g.vx -= g.vx * std::min(1.0f, drag * dt);
        g.vy -= g.vy * std::min(1.0f, drag * dt);
        g.vx = std::clamp(g.vx, -max_speed_x, max_speed_x);
        g.vy = std::clamp(g.vy, -max_speed_y, max_speed_y);
        g.x = wrap_x(g.x + g.vx * dt);
        g.y += g.vy * dt;
        const float min_y = floor_y + 2.4f, max_y = world_h - 2.0f;
        if (g.y < min_y || g.y > max_y)
        {
            g.y = std::clamp(g.y, min_y, max_y);
            g.vy = 0;
        }

        if (in.fire && g.fire_timer <= 0)
        {
            const float direction = g.facing_left ? -1.0f : 1.0f;
            g.shots.push_back({g.x + direction * (ship_half_width + 0.45f), g.y, direction * 65, 0, 1.5f, false});
            g.fire_timer = 0.18f;
            play(g.audio, g.audio.shot, g.x, 150);
        }
        if (in.bomb && g.bombs > 0 && g.smart_flash <= 0)
        {
            g.smart_flash = 0.8f;
            --g.bombs;
            for (Enemy &e : g.enemies)
                if (e.alive)
                    destroy_enemy(g, e);
            explosion(g, g.x, g.y, shot_c, 22);
        }

        g.enemy_timer -= dt;
        int living = 0;
        for (int enemy_index = 0; enemy_index < enemy_count; ++enemy_index)
        {
            Enemy &e = g.enemies[enemy_index];
            if (!e.alive)
                continue;
            ++living;
            e.phase += dt;
            const float dx = wrap_x(g.x - e.x);
            e.x = wrap_x(e.x + std::clamp(dx, -1.0f, 1.0f) * (e.bomber ? 5.0f : 2.8f) * dt);
            bool carrying = false;
            for (const Human &h : g.humans)
                carrying |= h.alive && h.abducted && h.carrier == enemy_index;
            e.y += carrying ? 3.0f * dt : std::sin(e.phase * (e.bomber ? 1.3f : 2.0f)) * 2.0f * dt;
            e.y = std::clamp(e.y, floor_y + 5, world_h - 5);
            if (distance_x(e.x, g.x) < 2.4f && std::fabs(e.y - g.y) < 2.4f && g.invulnerable <= 0)
            {
                destroy_enemy(g, e);
                kill_player(g);
                return;
            }
            if (e.bomber)
                for (Human &h : g.humans)
                    if (h.alive && !h.falling && !h.abducted && distance_x(h.x, e.x) < 2.0f && e.y < 42)
                    {
                        h.abducted = true;
                        h.carrier = enemy_index;
                    }
        }
        if (g.enemy_timer <= 0 && living > 0)
        {
            for (int i = 0; i < enemy_count; ++i)
                if (g.enemies[i].alive)
                {
                    const Enemy &e = g.enemies[i];
                    const float dx = wrap_x(g.x - e.x), dy = g.y - e.y;
                    const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
                    g.shots.push_back({e.x, e.y, dx / len * 15, dy / len * 15, 4.0f, true});
                    break;
                }
            g.enemy_timer = between(1.0f, 2.5f) / (1 + 0.12f * (g.wave - 1));
        }

        for (Shot &s : g.shots)
        {
            s.x = wrap_x(s.x + s.vx * dt);
            s.y += s.vy * dt;
            s.life -= dt;
            if (!s.enemy)
            {
                for (Enemy &e : g.enemies)
                    if (e.alive && distance_x(s.x, e.x) < 2.8f && std::fabs(s.y - e.y) < 2.4f)
                    {
                        destroy_enemy(g, e);
                        s.life = 0;
                        break;
                    }
            }
            else if (g.invulnerable <= 0 && distance_x(s.x, g.x) < 1.8f && std::fabs(s.y - g.y) < 1.8f)
            {
                kill_player(g);
                return;
            }
        }
        g.shots.erase(std::remove_if(g.shots.begin(), g.shots.end(), [](const Shot &s) { return s.life <= 0 || s.y < 0 || s.y > world_h; }),
                      g.shots.end());

        for (Human &h : g.humans)
        {
            if (!h.alive)
                continue;
            if (h.abducted)
            {
                const Enemy &carrier = g.enemies[h.carrier];
                if (carrier.alive)
                {
                    h.x = carrier.x;
                    h.y = carrier.y - 2.5f;
                    if (h.y >= world_h - 3.0f)
                    {
                        h.alive = false;
                        continue;
                    }
                }
                else
                {
                    h.abducted = false;
                    h.falling = true;
                    h.carrier = -1;
                }
            }
            if (distance_x(h.x, g.x) < 2.2f && std::fabs(g.y - h.y) < 3.2f)
            {
                rescue_human(g, h);
                continue;
            }
            if (h.falling)
            {
                h.y = std::max(floor_y, h.y - 10 * dt);
                if (h.y <= floor_y)
                {
                    h.alive = false;
                    explosion(g, h.x, floor_y + 1, human_c, 1.5f);
                }
            }
        }
        if (living == 0)
        {
            g.state = State::wave_clear;
            g.timer = 2.0f;
            g.score += 1000 + std::count_if(g.humans, g.humans + human_count, [](const Human &h) { return h.alive; }) * 100;
            g.hiscore = std::max(g.hiscore, g.score);
        }
    }

    Input bot_input(const Game &g)
    {
        Input in;
        in.fire = true;
        float target = 0, target_y = g.y, best = 1e9f;
        for (const Enemy &e : g.enemies)
            if (e.alive && distance_x(e.x, g.x) < best)
            {
                best = distance_x(e.x, g.x);
                target = e.x;
                target_y = e.y;
            }
        const float dx = wrap_x(target - g.x);
        in.left = dx < -2.0f;
        in.right = dx > 2.0f;
        in.up = target_y > g.y + 2.0f;
        in.down = target_y < g.y - 2.0f;
        in.fire = best < 35.0f && std::fabs(target_y - g.y) < 8.0f;
        return in;
    }

    struct Renderer
    {
        sl::Surface3D *surface = nullptr;
        sl::Mesh *cube = nullptr, *quad = nullptr;
        sl::Bitmap *puff = nullptr;
        sl::Bloom bloom;
        std::vector<glm::vec3> stars;
    };
    sl::Mesh *make_quad()
    {
        std::vector<sl::Vertex3D> vertices(4);
        const float corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (int i = 0; i < 4; ++i)
        {
            vertices[i].position[0] = corners[i][0];
            vertices[i].position[1] = corners[i][1];
            vertices[i].position[2] = 0;
            vertices[i].normal[2] = 1;
            vertices[i].uv[0] = 0.5f + corners[i][0] * 0.5f;
            vertices[i].uv[1] = 0.5f - corners[i][1] * 0.5f;
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
                const float dx = (x + 0.5f) / n * 2 - 1, dy = (y + 0.5f) / n * 2 - 1;
                const float a = std::clamp(1 - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                sl::putpixel(bitmap, x, y, {255, 255, 255, static_cast<Uint8>(255 * a * a * (3 - 2 * a))});
            }
        bitmap->smooth = true;
        return bitmap;
    }
    void add_box(std::vector<sl::InstanceData3D> &instances, glm::vec3 centre, glm::vec3 size, glm::mat4 rotation = glm::mat4(1))
    {
        sl::InstanceData3D instance;
        instance.model = glm::translate(glm::mat4(1), centre) * rotation * glm::scale(glm::mat4(1), size);
        instances.push_back(instance);
    }
    void draw_ship(Renderer &r, glm::vec3 pos, bool flip_x)
    {
        const std::string keys = "GMBORWN";
        const sl::Colour palette[] = {{165, 165, 185}, {255, 40, 230}, {50, 30, 190}, {255, 170, 20},
                                      {255, 40, 40}, {255, 255, 255}, {30, 200, 40}};
        std::array<std::vector<sl::InstanceData3D>, 7> pixels;
        const int h = static_cast<int>(ship_shape.size()), w = static_cast<int>(ship_shape.front().size());
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto k = keys.find(ship_shape[y][x]);
                if (k == std::string::npos)
                    continue;
                const float s = voxel * ship_scale;
                const glm::vec3 offset((x - (w - 1) * 0.5f) * s * (flip_x ? -1.0f : 1.0f), ((h - 1) * 0.5f - y) * s, 0);
                add_box(pixels[k], pos + offset, {s * 0.94f, s * 0.94f, 1.2f});
            }
        for (std::size_t i = 0; i < pixels.size(); ++i)
        {
            sl::Material material;
            material.lit = true;
            material.tint = palette[i];
            if (!pixels[i].empty())
                sl::draw_mesh_instanced(r.cube, pixels[i], material);
        }
    }
    void draw_sprite(Renderer &r, glm::vec3 pos, const std::vector<std::string> &shape, Colour colour, float scale = 1,
                     bool flip_x = false)
    {
        std::vector<sl::InstanceData3D> pixels;
        const int h = static_cast<int>(shape.size()), w = static_cast<int>(shape.front().size());
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (shape[y][x] == 'X')
                {
                    const float local_x = (x - (w - 1) * 0.5f) * voxel * scale * (flip_x ? -1.0f : 1.0f);
                    const glm::vec3 offset(local_x, ((h - 1) * 0.5f - y) * voxel * scale, 0);
                    add_box(pixels, pos + offset, {voxel * scale * 0.94f, voxel * scale * 0.94f, 1.2f});
                }
        sl::Material material;
        material.lit = true;
        material.tint = colours[colour];
        if (!pixels.empty())
            sl::draw_mesh_instanced(r.cube, pixels, material);
    }
    void draw_glow(Renderer &r, glm::vec3 pos, glm::vec2 size, sl::Colour colour, float alpha)
    {
        sl::Material m;
        m.lit = false;
        m.fog = false;
        m.blend = sl::MaterialBlend::additive;
        m.cull = sl::CullMode::none;
        m.texture = r.puff;
        m.tint = colour;
        m.tint.alpha = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255);
        glm::mat4 model = glm::translate(glm::mat4(1), pos);
        model = glm::scale(model, glm::vec3(size.x, size.y, 1));
        sl::draw_mesh(r.quad, model, m);
    }
    void draw_scene(Renderer &r, Game &g)
    {
        sl::Camera camera;
        camera.position = {g.x, 39, 138};
        camera.target = {g.x, 39, 0};
        camera.field_of_view = 34;
        camera.near_plane = 1;
        camera.far_plane = 350;
        if (!sl::begin_surface_3d(r.surface, camera))
            return;
        sl::clear_surface_3d({3, 5, 17});
        sl::Light3D light;
        light.direction = {-0.5f, -0.7f, -0.6f};
        light.ambient = 0.3f;
        sl::set_light_3d(light);
        for (const glm::vec3 &star : r.stars)
            draw_glow(r, {g.x + star.x, star.y, star.z}, {0.3f, 0.3f}, {160, 195, 255}, 0.6f);

        std::vector<sl::InstanceData3D> ground;
        for (float x = -150; x <= 150; x += 10)
            add_box(ground, {g.x + x, floor_y - 0.35f, -45}, {0.12f, 0.1f, 90});
        for (int z = 0; z < 11; ++z)
            add_box(ground, {g.x, floor_y - 0.35f, -4.0f - z * z * 1.2f}, {300, 0.1f, 0.12f});
        sl::Material ground_material;
        ground_material.lit = false;
        ground_material.tint = colours[ground_c];
        sl::draw_mesh_instanced(r.cube, ground, ground_material);

        const float x = near_x(g.x, g.x);
        const glm::vec3 ship_pos{x, g.y, 0};
        if (g.state == State::playing && (g.invulnerable <= 0 || static_cast<int>(g.time * 12) % 2 == 0))
        {
            draw_ship(r, ship_pos, g.facing_left);
            draw_glow(r, ship_pos + glm::vec3(g.facing_left ? -ship_half_width : ship_half_width, 0, 0.8f),
                      {1.2f, 1.2f}, {220, 250, 255}, 0.75f);
            draw_glow(r, ship_pos + glm::vec3(0, 0, -1), {4.2f, 2.8f}, colours[ship_c], 0.16f);
        }
        for (const Enemy &e : g.enemies)
            if (e.alive)
            {
                const glm::vec3 p{near_x(e.x, g.x), e.y, 0};
                draw_sprite(r, p, e.bomber ? bomber_shape : fighter_shape, e.bomber ? bomber_c : enemy_c, e.bomber ? 1.2f : 1);
                draw_glow(r, p + glm::vec3(0, 0, -1), {4.5f, 2.2f}, colours[e.bomber ? bomber_c : enemy_c], 0.16f);
            }
        for (const Human &h : g.humans)
            if (h.alive)
                draw_sprite(r, {near_x(h.x, g.x), h.y + (h.falling ? std::sin(g.time * 16) * 0.15f : 0), 0}, human_shape, human_c);

        for (const Shot &s : g.shots)
        {
            const glm::vec3 p{near_x(s.x, g.x), s.y, 0.8f};
            draw_glow(r, p, {s.enemy ? 1.0f : 3.2f, s.enemy ? 1.0f : 0.7f}, s.enemy ? colours[enemy_c] : colours[shot_c], 0.95f);
        }
        for (const Glow &gl : g.glows)
        {
            const float t = gl.age / gl.life, size = gl.size0 + (gl.size1 - gl.size0) * t;
            draw_glow(r, {near_x(gl.pos.x, g.x), gl.pos.y, gl.pos.z}, {size, size}, gl.colour, gl.strength * (1 - t) * (1 - t));
        }
        std::array<std::vector<sl::InstanceData3D>, colour_count> debris;
        for (const sl::RigidBox3D &body : g.debris.bodies())
        {
            const auto colour = std::min<std::uint32_t>(body.user, colour_count - 1);
            sl::InstanceData3D instance;
            glm::mat4 model = sl::RigidBoxWorld3D::model_matrix(body);
            model[3].x = near_x(body.position.x, g.x);
            instance.model = model;
            debris[colour].push_back(instance);
        }
        for (int c = 0; c < colour_count; ++c)
            if (!debris[c].empty())
            {
                sl::Material material;
                material.tint = colours[c];
                sl::draw_mesh_instanced(r.cube, debris[c], material);
            }
        if (g.smart_flash > 0)
            draw_glow(r, {g.x, g.y, 1}, {70 * (1 - g.smart_flash), 70 * (1 - g.smart_flash)}, {180, 240, 255}, g.smart_flash);
        sl::end_surface_3d();
        r.bloom.apply(sl::surface_3d_bitmap(r.surface), 0, 0, window_w, window_h);
    }
    // Scanner across the top: the whole wrapping world, centred on the player.
    void draw_radar(const Game &g)
    {
        constexpr float left = window_w * 0.5f - 190, right = window_w * 0.5f + 190, top = 58, bottom = 100;
        sl::rectfill(sl::screen, left, top, right, bottom, {4, 10, 28, 200});
        sl::rect(sl::screen, left, top, right, bottom, {60, 120, 255});
        const float view = (right - left) * 34.0f / world_w;
        sl::rect(sl::screen, (left + right) * 0.5f - view * 0.5f, top + 1, (left + right) * 0.5f + view * 0.5f, bottom - 1, {40, 80, 160});
        auto blip = [&](float x, float y, sl::Colour c, float size) {
            const float px = (left + right) * 0.5f + wrap_x(x - g.x) / world_w * (right - left);
            const float py = bottom - 3 - std::clamp((y - floor_y) / (world_h - floor_y), 0.0f, 1.0f) * (bottom - top - 6);
            sl::rectfill(sl::screen, px - size, py - size, px + size, py + size, c);
        };
        for (const Human &h : g.humans)
            if (h.alive)
                blip(h.x, h.y, {110, 255, 110}, 1.5f);
        for (const Enemy &e : g.enemies)
            if (e.alive)
                blip(e.x, e.y, colours[e.bomber ? bomber_c : enemy_c], 2.0f);
        if (g.state == State::playing)
            blip(g.x, g.y, {255, 255, 255}, 2.5f);
    }
    void draw_hud(const Game &g)
    {
        const sl::Colour white{235, 240, 255}, cyan{70, 235, 255}, pink{255, 90, 220}, lime{180, 255, 90};
        sl::gprintf(24, 10, pink, "SCORE");
        sl::gprintf(24, 30, white, "%06d", g.score);
        sl::gprintf_center(10, pink, "HI-SCORE");
        sl::gprintf_center(30, white, "%06d", g.hiscore);
        sl::gprintf(window_w - 150, 10, pink, "WAVE");
        sl::gprintf(window_w - 150, 30, white, "%d", std::max(g.wave, 1));
        if (g.state != State::title)
            draw_radar(g);
        sl::gprintf(24, window_h - 30, cyan, "SHIPS  %d   BOMBS  %d", g.lives, g.bombs);
        sl::gprintf(window_w - 170, window_h - 30, lime, "RESCUED  %d", g.rescued);
        if (g.state == State::title)
        {
            sl::gprintf_center(200, cyan, "E X D E F E N D E R");
            sl::gprintf_center(245, white, "Press ENTER or SPACE to launch");
            sl::gprintf_center(274, white, "ARROWS move up / down / left / right");
            sl::gprintf_center(300, white, "SPACE fire  Z smart bomb  rescue falling people");
        }
        else if (g.state == State::game_over)
        {
            sl::gprintf_center(245, pink, "G A M E   O V E R");
            sl::gprintf_center(276, white, "Press ENTER to launch again");
        }
        else if (g.state == State::wave_clear)
            sl::gprintf_center(280, lime, "SECTOR CLEARED");
    }
    void shutdown_audio(Audio &a)
    {
        if (a.ready)
            sl::audio_fx_shutdown();
    }
} // namespace

int main(int argc, char *argv[])
{
    bool autoplay = false;
    std::string shot_path;
    float shot_at = 8;
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
    sl::set_window_title("exdefender");
    Renderer renderer;
    renderer.surface = sl::create_surface_3d(window_w, window_h, 4);
    renderer.cube = sl::create_cube_mesh(1);
    renderer.quad = make_quad();
    renderer.puff = make_puff();
    if (!renderer.surface || !renderer.cube || !renderer.quad || !renderer.puff || !renderer.bloom.initialise())
    {
        std::fprintf(stderr, "exdefender needs 3D surfaces and bloom, which this backend doesn't provide.\n");
        sl::shutdown();
        return -1;
    }
    renderer.bloom.set_threshold(0.5f);
    renderer.bloom.set_intensity(1.3f);
    renderer.bloom.set_radius(3);
    std::srand(19);
    for (int i = 0; i < 240; ++i)
        renderer.stars.push_back({between(-150, 150), between(8, 88), between(-160, -30)});

    static Game game;
    init_audio(game.audio);
    game.debris.floor_height = floor_y;
    game.debris.gravity = {0, -22, 0};
    game.debris.max_bodies = 3000;
    reset_wave(game);
    game.wave = 0;
    if (autoplay)
        new_game(game);

    sl::set_fps(60);
    bool running = true, saved = false;
    float last = static_cast<float>(sl::time_ms()) * 0.001f, elapsed = 0;
    int game_over_frames = 0;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down &&
                     (event.key() == sl::Event::Key::return_key || event.key() == sl::Event::Key::keypad_enter ||
                      (event.key() == sl::Event::Key::space && game.state == State::title)))
                if (game.state == State::title || game.state == State::game_over)
                    new_game(game);
            sl::display_handle_event(event);
        }
        const float now = static_cast<float>(sl::time_ms()) * 0.001f;
        const float dt = std::min(now - last, 0.05f);
        last = now;
        elapsed += dt;
        Input input;
        if (autoplay)
        {
            game_over_frames = game.state == State::game_over ? game_over_frames + 1 : 0;
            if (game_over_frames > 120)
                new_game(game);
            input = bot_input(game);
        }
        else
        {
            input.left = sl::key_down(sl::Event::Key::arrow_left);
            input.right = sl::key_down(sl::Event::Key::arrow_right);
            input.up = sl::key_down(sl::Event::Key::arrow_up);
            input.down = sl::key_down(sl::Event::Key::arrow_down);
            input.fire = sl::key_down(sl::Event::Key::space);
            input.bomb = sl::key_down(sl::Event::Key::letter_z);
        }
        update(game, dt, input);
        draw_scene(renderer, game);
        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(renderer.surface), 0, 0);
        draw_hud(game);
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
    shutdown_audio(game.audio);
    renderer.bloom.shutdown();
    sl::destroy_bitmap(renderer.puff);
    sl::destroy_mesh(renderer.quad);
    sl::destroy_mesh(renderer.cube);
    sl::destroy_surface_3d(renderer.surface);
    sl::shutdown();
    return 0;
}
