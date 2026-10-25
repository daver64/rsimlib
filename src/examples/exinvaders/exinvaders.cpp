// exinvaders: classic Space Invaders built entirely from glowing voxels. The play field is the classic 2D one, seen
// side-on, but every invader, the cannon and the three shields are 3D voxel boxes. Hits shatter voxels into
// RigidBoxWorld3D debris that tumbles, bounces and fades while an additive flash blooms where it happened.
//
// Left/Right move, Space fires, Enter starts or restarts, Escape quits.
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
    constexpr float V = 0.4f;           // voxel size
    constexpr int cols = 11, rows = 5;  // invader formation
    constexpr float col_gap = 7.2f, row_gap = 5.4f;
    constexpr float field = 50.0f;      // half width of the play field
    constexpr float player_y = 2.0f;
    constexpr float floor_y = 0.0f;

    // Voxel colour groups, drawn as one instanced batch each.
    enum Palette { squid_c, crab_c, octopus_c, player_c, shield_c, ufo_c, palette_count };
    const sl::Colour palette[palette_count] = {{255, 60, 230}, {40, 230, 255}, {255, 150, 30},
                                               {170, 220, 255}, {150, 255, 40}, {255, 50, 80}};

    struct Sprite
    {
        int w, h;
        std::vector<std::string> frames[2];
        Palette colour;
        int points;
    };

    const Sprite sprites[] = {
        {8, 8,
         {{"...XX...", "..XXXX..", ".XXXXXX.", "XX.XX.XX", "XXXXXXXX", "..X..X..", ".X.XX.X.", "X.X..X.X"},
          {"...XX...", "..XXXX..", ".XXXXXX.", "XX.XX.XX", "XXXXXXXX", ".X.XX.X.", "X......X", ".X....X."}},
         squid_c, 30},
        {11, 8,
         {{"..X.....X..", "...X...X...", "..XXXXXXX..", ".XX.XXX.XX.", "XXXXXXXXXXX", "X.XXXXXXX.X", "X.X.....X.X", "...XX.XX..."},
          {"..X.....X..", "X..X...X..X", "X.XXXXXXX.X", "XXX.XXX.XXX", "XXXXXXXXXXX", ".XXXXXXXXX.", "..X.....X..", ".X.......X."}},
         crab_c, 20},
        {12, 8,
         {{"....XXXX....", ".XXXXXXXXXX.", "XXXXXXXXXXXX", "XXX..XX..XXX", "XXXXXXXXXXXX", "...XX..XX...", "..XX.XX.XX..", "XX........XX"},
          {"....XXXX....", ".XXXXXXXXXX.", "XXXXXXXXXXXX", "XXX..XX..XXX", "XXXXXXXXXXXX", "..XXX..XXX..", ".XX..XX..XX.", "..XX....XX.."}},
         octopus_c, 10},
    };
    const Sprite cannon_sprite = {13, 8,
                                  {{"......X......", ".....XXX.....", ".....XXX.....", ".XXXXXXXXXXX.", "XXXXXXXXXXXXX", "XXXXXXXXXXXXX", "XXXXXXXXXXXXX", "XXXXXXXXXXXXX"}, {}},
                                  player_c, 0};
    const Sprite ufo_sprite = {16, 7,
                               {{".....XXXXXX.....", "...XXXXXXXXXX...", "..XXXXXXXXXXXX..", ".XX.XX.XX.XX.XX.", "XXXXXXXXXXXXXXXX", "..XXX..XX..XXX..", "...X........X..."}, {}},
                               ufo_c, 0};

    int sprite_for_row(int row) { return row == 0 ? 0 : (row < 3 ? 1 : 2); }

    float uniform() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
    float between(float a, float b) { return a + (b - a) * uniform(); }

    // Voxel centres of a sprite frame, relative to the sprite's centre.
    std::vector<glm::vec3> voxel_offsets(const Sprite &s, int frame)
    {
        std::vector<glm::vec3> out;
        const auto &rowsv = s.frames[s.frames[frame].empty() ? 0 : frame];
        for (int j = 0; j < s.h; ++j)
            for (int i = 0; i < s.w; ++i)
                if (rowsv[j][i] == 'X')
                    out.push_back({(i - (s.w - 1) * 0.5f) * V, ((s.h - 1) * 0.5f - j) * V, 0.0f});
        return out;
    }

    struct Shield
    {
        static constexpr int w = 22, h = 16;
        float x = 0, y = 0;
        bool v[h][w];
        void rebuild()
        {
            for (int j = 0; j < h; ++j)
                for (int i = 0; i < w; ++i)
                {
                    bool on = true;
                    if (j < 4) // rounded shoulders
                        on = i >= 4 - j && i < w - 4 + j;
                    if (j >= 12 && i >= 8 && i < 14) // notch for the cannon
                        on = false;
                    v[j][i] = on;
                }
        }
        glm::vec3 position(int i, int j) const { return {x + (i - (w - 1) * 0.5f) * V, y + ((h - 1) * 0.5f - j) * V, 0.0f}; }
        // Index of the voxel under a point, or false.
        bool at(float px, float py, int &i, int &j) const
        {
            i = static_cast<int>(std::floor((px - x) / V + w * 0.5f));
            j = static_cast<int>(std::floor((y - py) / V + h * 0.5f));
            return i >= 0 && i < w && j >= 0 && j < h && v[j][i];
        }
    };

    struct Glow
    {
        glm::vec3 pos, vel{0.0f};
        float age = 0, life = 0.5f, size0 = 1, size1 = 1, strength = 1;
        sl::Colour colour;
    };

    struct Shot
    {
        float x, y, vy, phase = 0;
        bool player;
    };

    struct Audio
    {
        sl::Sample *shoot = nullptr, *kill = nullptr, *boom = nullptr, *bomb = nullptr, *ufo = nullptr, *ufo_hit = nullptr, *start = nullptr;
        sl::Sample *march[4] = {nullptr, nullptr, nullptr, nullptr};
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
        a.shoot = synth(0.2f, {0.001f, 0.05f, 0.3f, 0.08f}, {{W::square, 1400.0f, 0.4f, {1.0f, 0.15f}}, {W::saw, 700.0f, 0.2f, {1.0f, 0.2f}}});
        a.kill = synth(0.35f, {0.001f, 0.12f, 0.3f, 0.15f}, {{W::noise, 1.0f, 0.5f, {}}, {W::square, 600.0f, 0.3f, {1.0f, 0.1f}}});
        a.boom = synth(1.1f, {0.002f, 0.4f, 0.4f, 0.5f}, {{W::noise, 1.0f, 0.7f, {}}, {W::saw, 160.0f, 0.5f, {1.0f, 0.1f}}, {W::sine, 70.0f, 0.6f, {1.0f, 0.4f}}});
        a.bomb = synth(0.12f, {0.001f, 0.04f, 0.3f, 0.05f}, {{W::noise, 1.0f, 0.4f, {}}, {W::triangle, 300.0f, 0.3f, {1.0f, 0.4f}}});
        a.ufo = synth(0.11f, {0.005f, 0.03f, 0.7f, 0.03f}, {{W::sine, 900.0f, 0.4f, {0.8f, 1.2f}}, {W::triangle, 1350.0f, 0.15f, {0.8f, 1.2f}}});
        a.ufo_hit = synth(0.8f, {0.002f, 0.2f, 0.4f, 0.3f}, {{W::square, 1800.0f, 0.4f, {1.0f, 0.1f}}, {W::noise, 1.0f, 0.3f, {}}});
        a.start = synth(0.5f, {0.01f, 0.1f, 0.5f, 0.2f}, {{W::square, 220.0f, 0.3f, {1.0f, 2.0f}}, {W::triangle, 440.0f, 0.3f, {1.0f, 2.0f}}});
        const float notes[4] = {65.4f, 58.3f, 51.9f, 49.0f};
        for (int i = 0; i < 4; ++i)
            a.march[i] = synth(0.12f, {0.002f, 0.04f, 0.5f, 0.05f}, {{W::square, notes[i], 0.5f, {}}, {W::sine, notes[i] * 0.5f, 0.5f, {}}});
        a.ready = a.shoot && a.kill && a.boom && a.bomb && a.ufo && a.ufo_hit && a.start && a.march[3];
    }

    void play(const Audio &a, sl::Sample *s, float x, int volume = 20, int frequency = 1000)
    {
        if (a.ready && s)
            sl::play_sample(s, volume, std::clamp(static_cast<int>((x + field) / (field * 2.0f) * 255.0f), 0, 255), frequency);
    }

    enum class State { title, playing, player_dead, wave_clear, game_over };

    struct Game
    {
        State state = State::title;
        int score = 0, hiscore = 0, lives = 3, wave = 0;
        bool alive[rows][cols];
        int alive_count = 0;
        float fx = 0, fy = 0;
        int dir = 1, steps = 0;
        float step_timer = 0, bomb_timer = 1.0f;
        float player_x = 0, invuln = 0, state_timer = 0, ufo_timer = 20.0f, ufo_x = 0, ufo_blip = 0, shake = 0;
        int ufo_dir = 1;
        bool ufo_on = false;
        std::string message;
        float message_time = 0;
        Shield shields[3];
        std::vector<Shot> shots;
        std::vector<Glow> glows;
        sl::RigidBoxWorld3D debris;
        Audio audio;
        float time = 0;
    };

    float invader_x(const Game &g, int c) { return g.fx + (c - cols / 2) * col_gap; }
    float invader_y(const Game &g, int r) { return g.fy - r * row_gap; }

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

    // Shatter voxels into tumbling debris pushed away from a blast point.
    void shatter(Game &g, const std::vector<glm::vec3> &positions, Palette colour, glm::vec3 blast, float radius, float strength)
    {
        sl::RigidBoxWorld3D burst;
        burst.max_bodies = 100000;
        for (const glm::vec3 &p : positions)
        {
            sl::RigidBox3D body;
            body.position = p;
            body.half_extents = {V * 0.46f, V * 0.46f, V * 1.4f};
            body.mass = 1.0f;
            body.restitution = 0.45f;
            body.friction = 0.4f;
            body.lifetime = between(1.2f, 2.2f);
            body.linear_damping = 0.4f;
            body.user = static_cast<std::uint32_t>(colour);
            burst.add(body);
        }
        burst.explode(blast, radius, strength, 9.0f);
        for (sl::RigidBox3D &b : burst.bodies())
        {
            b.velocity.z += between(-4.0f, 10.0f);
            g.debris.bodies().push_back(b);
        }
    }

    void explosion_fx(Game &g, glm::vec3 at, sl::Colour colour, float size)
    {
        add_glow(g, at, {0, 0, 0}, 0.55f, size * 0.6f, size * 2.2f, {255, 255, 255}, 1.0f);
        add_glow(g, at, {0, 0, 0}, 0.8f, size * 0.8f, size * 3.0f, colour, 0.9f);
        for (int i = 0; i < 10; ++i)
        {
            const float a = uniform() * 6.2832f, s = between(6.0f, 22.0f);
            add_glow(g, at, {std::cos(a) * s, std::sin(a) * s, between(-4.0f, 8.0f)}, between(0.4f, 0.9f), 1.0f, 0.2f, colour, 1.0f);
        }
    }

    void destroy_shield_voxels(Game &g, Shield &sh, float x, float y, float radius, glm::vec3 blast)
    {
        std::vector<glm::vec3> gone;
        for (int j = 0; j < Shield::h; ++j)
            for (int i = 0; i < Shield::w; ++i)
            {
                if (!sh.v[j][i])
                    continue;
                const glm::vec3 p = sh.position(i, j);
                const float dx = p.x - x, dy = p.y - y;
                // Ragged edge: the radius wobbles per voxel so craters aren't perfect circles.
                if (dx * dx + dy * dy <= radius * radius * (0.7f + 0.5f * uniform()))
                {
                    sh.v[j][i] = false;
                    gone.push_back(p);
                }
            }
        if (!gone.empty())
            shatter(g, gone, shield_c, blast, radius * 4.0f, 14.0f);
    }

    // Return true if a point lands in a shield voxel, and carve the shield there.
    bool hit_shield(Game &g, float x, float y, float radius, glm::vec3 blast)
    {
        for (Shield &sh : g.shields)
        {
            int i, j;
            if (sh.at(x, y, i, j))
            {
                destroy_shield_voxels(g, sh, x, y, radius, blast);
                add_glow(g, {x, y, 0.5f}, {0, 0, 0}, 0.3f, 1.0f, 2.8f, palette[shield_c], 0.8f);
                return true;
            }
        }
        return false;
    }

    void start_wave(Game &g)
    {
        ++g.wave;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                g.alive[r][c] = true;
        g.alive_count = rows * cols;
        g.fx = 0;
        g.fy = 62.0f - std::min(g.wave - 1, 5) * 3.0f;
        g.dir = 1;
        g.steps = 0;
        g.step_timer = 0.5f;
        g.shots.clear();
        const float xs[3] = {-28.0f, 0.0f, 28.0f};
        for (int i = 0; i < 3; ++i)
        {
            g.shields[i].x = xs[i];
            g.shields[i].y = 11.0f;
            g.shields[i].rebuild();
        }
        g.bomb_timer = 1.5f;
    }

    void new_game(Game &g)
    {
        g.score = 0;
        g.lives = 3;
        g.wave = 0;
        g.player_x = 0;
        g.invuln = 1.0f;
        g.ufo_on = false;
        g.ufo_timer = between(15.0f, 25.0f);
        g.debris.clear();
        g.glows.clear();
        start_wave(g);
        g.state = State::playing;
        play(g.audio, g.audio.start, 0.0f);
    }

    void kill_player(Game &g)
    {
        std::vector<glm::vec3> voxels = voxel_offsets(cannon_sprite, 0);
        for (glm::vec3 &p : voxels)
            p += glm::vec3(g.player_x, player_y, 0.0f);
        shatter(g, voxels, player_c, {g.player_x, player_y - 1.0f, -2.0f}, 9.0f, 38.0f);
        explosion_fx(g, {g.player_x, player_y, 0.5f}, palette[player_c], 6.0f);
        play(g.audio, g.audio.boom, g.player_x, 255);
        g.shake = 0.9f;
        g.shots.clear();
        --g.lives;
        g.state = State::player_dead;
        g.state_timer = 2.2f;
    }

    void kill_invader(Game &g, int r, int c)
    {
        const Sprite &s = sprites[sprite_for_row(r)];
        const float x = invader_x(g, c), y = invader_y(g, r);
        std::vector<glm::vec3> voxels = voxel_offsets(s, g.steps & 1);
        for (glm::vec3 &p : voxels)
            p += glm::vec3(x, y, 0.0f);
        shatter(g, voxels, s.colour, {x, y - 1.0f, -2.0f}, 6.0f, 26.0f);
        explosion_fx(g, {x, y, 0.5f}, palette[s.colour], 3.5f);
        g.alive[r][c] = false;
        --g.alive_count;
        g.score += s.points;
        g.hiscore = std::max(g.hiscore, g.score);
        play(g.audio, g.audio.kill, x, 210);
        g.shake = std::max(g.shake, 0.12f);
    }

    void kill_ufo(Game &g)
    {
        std::vector<glm::vec3> voxels = voxel_offsets(ufo_sprite, 0);
        for (glm::vec3 &p : voxels)
            p += glm::vec3(g.ufo_x, 69.0f, 0.0f);
        shatter(g, voxels, ufo_c, {g.ufo_x, 68.0f, -2.0f}, 10.0f, 30.0f);
        explosion_fx(g, {g.ufo_x, 69.0f, 0.5f}, palette[ufo_c], 5.0f);
        const int bonus[4] = {50, 100, 150, 300};
        const int points = bonus[std::rand() % 4];
        g.score += points;
        g.hiscore = std::max(g.hiscore, g.score);
        g.message = "MYSTERY SHIP  +" + std::to_string(points);
        g.message_time = 2.5f;
        g.ufo_on = false;
        g.ufo_timer = between(20.0f, 32.0f);
        play(g.audio, g.audio.ufo_hit, g.ufo_x, 230);
    }

    void fire_bomb(Game &g)
    {
        std::vector<int> candidates;
        for (int c = 0; c < cols; ++c)
            for (int r = rows - 1; r >= 0; --r)
                if (g.alive[r][c])
                {
                    candidates.push_back(r * cols + c);
                    break;
                }
        if (candidates.empty())
            return;
        // One bomb in three is aimed at the player's column.
        int pick = candidates[std::rand() % candidates.size()];
        if (std::rand() % 3 == 0)
        {
            float best = 1e9f;
            for (int id : candidates)
            {
                const float d = std::fabs(invader_x(g, id % cols) - g.player_x);
                if (d < best)
                {
                    best = d;
                    pick = id;
                }
            }
        }
        const int r = pick / cols, c = pick % cols;
        g.shots.push_back({invader_x(g, c), invader_y(g, r) - 2.0f, -(20.0f + std::min(g.wave, 6) * 2.0f), uniform() * 6.0f, false});
        play(g.audio, g.audio.bomb, invader_x(g, c), 110);
    }

    void step_formation(Game &g)
    {
        float lo = 1e9f, hi = -1e9f, bottom = 1e9f;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (g.alive[r][c])
                {
                    const float half = sprites[sprite_for_row(r)].w * V * 0.5f;
                    lo = std::min(lo, invader_x(g, c) - half);
                    hi = std::max(hi, invader_x(g, c) + half);
                    bottom = std::min(bottom, invader_y(g, r));
                }
        const float step = 1.6f;
        if ((g.dir > 0 && hi + step > field) || (g.dir < 0 && lo - step < -field))
        {
            g.dir = -g.dir;
            g.fy -= 2.6f;
        }
        else
            g.fx += g.dir * step;
        ++g.steps;
        play(g.audio, g.audio.march[g.steps & 3], 0.0f, 150);

        // Invaders that have reached the shields eat them.
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (g.alive[r][c])
                {
                    const float x = invader_x(g, c), y = invader_y(g, r);
                    const float half = sprites[sprite_for_row(r)].w * V * 0.5f;
                    for (Shield &sh : g.shields)
                        if (std::fabs(x - sh.x) < half + 4.4f && std::fabs(y - sh.y) < 6.4f)
                            for (float px = x - half; px <= x + half; px += V)
                                for (float py = y - 1.6f; py <= y + 1.6f; py += V)
                                {
                                    int i, j;
                                    if (sh.at(px, py, i, j))
                                    {
                                        sh.v[j][i] = false;
                                        shatter(g, {sh.position(i, j)}, shield_c, {px, py, -1.5f}, 4.0f, 10.0f);
                                    }
                                }
                }
        if (bottom - 1.6f <= player_y + 1.6f)
        {
            g.lives = 1;
            kill_player(g);
        }
    }

    struct Input
    {
        bool left = false, right = false, fire = false;
    };

    // A simple bot for --autoplay: dodge bombs, line up under the nearest invader and shoot.
    Input bot_input(const Game &g)
    {
        Input in;
        float dodge = 0;
        for (const Shot &s : g.shots)
            if (!s.player && s.y < 16.0f && s.y > 2.0f && std::fabs(s.x - g.player_x) < 3.6f)
                dodge = g.player_x >= s.x ? 1.0f : -1.0f;
        if (g.player_x > field - 6.0f && dodge > 0)
            dodge = -1;
        if (g.player_x < -field + 6.0f && dodge < 0)
            dodge = 1;
        if (dodge != 0)
        {
            in.right = dodge > 0;
            in.left = dodge < 0;
            return in;
        }
        float best = 1e9f, target = 0;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (g.alive[r][c])
                {
                    const float d = std::fabs(invader_x(g, c) - g.player_x);
                    if (d < best)
                    {
                        best = d;
                        target = invader_x(g, c) + g.dir * 1.0f;
                    }
                }
        if (target > g.player_x + 0.6f)
            in.right = true;
        else if (target < g.player_x - 0.6f)
            in.left = true;
        in.fire = std::fabs(target - g.player_x) < 1.8f;
        return in;
    }

    void update(Game &g, float dt, const Input &in)
    {
        g.time += dt;
        g.shake = std::max(0.0f, g.shake - dt);
        g.message_time = std::max(0.0f, g.message_time - dt);
        g.debris.step(dt, 2);
        for (Glow &gl : g.glows)
        {
            gl.age += dt;
            gl.pos += gl.vel * dt;
            gl.vel *= std::max(0.0f, 1.0f - 2.0f * dt);
        }
        g.glows.erase(std::remove_if(g.glows.begin(), g.glows.end(), [](const Glow &gl) { return gl.age >= gl.life; }), g.glows.end());

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
                    g.state = State::playing;
                    g.player_x = 0;
                    g.invuln = 2.0f;
                }
            }
            return;
        }
        if (g.state == State::wave_clear)
        {
            g.state_timer -= dt;
            if (g.state_timer <= 0)
            {
                start_wave(g);
                g.state = State::playing;
                g.message = "WAVE " + std::to_string(g.wave);
                g.message_time = 2.0f;
            }
            return;
        }

        g.invuln = std::max(0.0f, g.invuln - dt);
        const float speed = 38.0f;
        if (in.left)
            g.player_x -= speed * dt;
        if (in.right)
            g.player_x += speed * dt;
        g.player_x = std::clamp(g.player_x, -field + 3.0f, field - 3.0f);

        bool missile_up = false;
        for (const Shot &s : g.shots)
            missile_up |= s.player;
        if (in.fire && !missile_up)
        {
            g.shots.push_back({g.player_x, player_y + 2.4f, 62.0f, 0, true});
            play(g.audio, g.audio.shoot, g.player_x, 200);
        }

        // Formation march: faster as invaders die.
        g.step_timer -= dt;
        if (g.step_timer <= 0 && g.alive_count > 0)
        {
            step_formation(g);
            if (g.state != State::playing)
                return;
            g.step_timer = 0.04f + g.alive_count * 0.011f / (1.0f + 0.12f * std::min(g.wave - 1, 5));
        }

        g.bomb_timer -= dt;
        int bombs = 0;
        for (const Shot &s : g.shots)
            bombs += s.player ? 0 : 1;
        if (g.bomb_timer <= 0)
        {
            if (bombs < 2 + std::min(g.wave, 3) && g.alive_count > 0)
                fire_bomb(g);
            g.bomb_timer = between(0.4f, 1.2f) / (1.0f + 0.15f * (g.wave - 1));
        }

        // Mystery ship.
        if (!g.ufo_on)
        {
            g.ufo_timer -= dt;
            if (g.ufo_timer <= 0 && g.alive_count > 8)
            {
                g.ufo_on = true;
                g.ufo_dir = (std::rand() & 1) ? 1 : -1;
                g.ufo_x = -g.ufo_dir * (field + 6.0f);
            }
        }
        else
        {
            g.ufo_x += g.ufo_dir * 14.0f * dt;
            g.ufo_blip -= dt;
            if (g.ufo_blip <= 0)
            {
                play(g.audio, g.audio.ufo, g.ufo_x, 120);
                g.ufo_blip = 0.13f;
            }
            if (std::fabs(g.ufo_x) > field + 8.0f)
            {
                g.ufo_on = false;
                g.ufo_timer = between(20.0f, 32.0f);
            }
        }

        // Projectiles, moved in small steps so fast shots can't skip through voxels.
        for (Shot &s : g.shots)
        {
            float remaining = std::fabs(s.vy) * dt;
            const float sign = s.vy > 0 ? 1.0f : -1.0f;
            s.phase += dt;
            while (remaining > 0 && s.y > -50.0f)
            {
                const float d = std::min(remaining, 0.2f);
                remaining -= d;
                s.y += sign * d;
                if (s.player)
                {
                    if (s.y > 74.0f)
                    {
                        add_glow(g, {s.x, 72.0f, 0.5f}, {0, 0, 0}, 0.4f, 1.0f, 4.0f, {255, 255, 255});
                        s.y = -100;
                        break;
                    }
                    if (hit_shield(g, s.x, s.y, 1.0f, {s.x, s.y - 1.5f, -1.0f}))
                    {
                        s.y = -100;
                        break;
                    }
                    bool done = false;
                    for (int r = 0; r < rows && !done; ++r)
                        for (int c = 0; c < cols && !done; ++c)
                        {
                            if (!g.alive[r][c])
                                continue;
                            const Sprite &sp = sprites[sprite_for_row(r)];
                            if (std::fabs(s.x - invader_x(g, c)) < sp.w * V * 0.5f && std::fabs(s.y - invader_y(g, r)) < sp.h * V * 0.5f)
                            {
                                kill_invader(g, r, c);
                                done = true;
                            }
                        }
                    if (!done && g.ufo_on && std::fabs(s.x - g.ufo_x) < 3.2f && std::fabs(s.y - 69.0f) < 1.5f)
                    {
                        kill_ufo(g);
                        done = true;
                    }
                    if (done)
                    {
                        s.y = -100;
                        break;
                    }
                }
                else
                {
                    if (s.y < floor_y + 0.4f)
                    {
                        add_glow(g, {s.x, 0.6f, 0.5f}, {0, 0, 0}, 0.35f, 0.8f, 3.0f, {255, 120, 60});
                        s.y = -100;
                        break;
                    }
                    if (hit_shield(g, s.x, s.y, 1.3f, {s.x, s.y + 1.5f, -1.0f}))
                    {
                        s.y = -100;
                        break;
                    }
                    if (g.invuln <= 0 && std::fabs(s.x - g.player_x) < 2.6f && std::fabs(s.y - player_y) < 1.7f)
                    {
                        s.y = -100;
                        kill_player(g);
                        break;
                    }
                }
            }
            if (s.y > -50.0f)
            {
                if (s.player)
                    add_glow(g, {s.x, s.y - 1.0f, 0.0f}, {0, -6.0f, 0}, 0.28f, 0.9f, 0.2f, {120, 230, 255}, 0.8f);
                else
                    add_glow(g, {s.x + std::sin(s.phase * 18.0f) * 0.5f, s.y + 0.8f, 0.0f}, {0, 5.0f, 0}, 0.3f, 0.9f, 0.2f, {255, 90, 70}, 0.7f);
            }
        }
        // A player missile and a bomb that meet cancel in a small flash.
        for (Shot &m : g.shots)
            if (m.player && m.y > -50.0f)
                for (Shot &b : g.shots)
                    if (!b.player && b.y > -50.0f && std::fabs(b.x - m.x) < 1.2f && std::fabs(b.y - m.y) < 2.4f)
                    {
                        add_glow(g, {m.x, m.y, 0.5f}, {0, 0, 0}, 0.4f, 1.0f, 4.5f, {255, 255, 255});
                        explosion_fx(g, {m.x, m.y, 0.5f}, {255, 200, 120}, 1.5f);
                        m.y = b.y = -100;
                    }
        g.shots.erase(std::remove_if(g.shots.begin(), g.shots.end(), [](const Shot &s) { return s.y <= -50.0f; }), g.shots.end());

        if (g.state == State::playing && g.alive_count == 0)
        {
            g.state = State::wave_clear;
            g.state_timer = 2.4f;
            g.shots.clear();
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
        std::vector<glm::vec3> grid_boxes_pos, grid_boxes_size;
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
        for (int i = 0; i < 260; ++i)
        {
            Renderer::Star s;
            s.pos = {between(-150.0f, 150.0f), between(-20.0f, 130.0f), between(-170.0f, -40.0f)};
            s.size = between(0.15f, 0.5f);
            s.phase = between(0.0f, 6.28f);
            const float t = uniform();
            s.colour = t < 0.6f ? sl::Colour{200, 220, 255} : (t < 0.8f ? sl::Colour{255, 170, 240} : sl::Colour{150, 200, 255});
            r.stars.push_back(s);
        }
        // Receding neon floor grid.
        for (float x = -96.0f; x <= 96.0f; x += 8.0f)
        {
            r.grid_boxes_pos.push_back({x, floor_y - 0.2f, -80.0f});
            r.grid_boxes_size.push_back({0.14f, 0.1f, 160.0f});
        }
        const float zs[] = {-3, -7, -12, -19, -28, -40, -56, -78, -110, -150};
        for (float z : zs)
        {
            r.grid_boxes_pos.push_back({0.0f, floor_y - 0.2f, z});
            r.grid_boxes_size.push_back({200.0f, 0.1f, 0.14f + std::fabs(z) * 0.002f});
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

    void draw_scene(Renderer &r, Game &g)
    {
        const float shake = g.shake > 0 ? g.shake * 1.2f : 0.0f;
        sl::Camera camera;
        const float sway = std::sin(g.time * 0.23f);
        camera.position = {g.player_x * 0.2f + sway * 11.0f + between(-shake, shake), 44.0f + std::sin(g.time * 0.17f) * 3.0f + between(-shake, shake), 108.0f};
        camera.target = {0.0f, 31.0f, 0.0f};
        camera.field_of_view = 40.0f;
        camera.near_plane = 1.0f;
        camera.far_plane = 500.0f;
        if (!sl::begin_surface_3d(r.surface, camera))
            return;
        sl::clear_surface_3d({4, 3, 14});
        sl::Light3D light;
        light.direction = {-0.55f, -0.75f, -0.5f};
        light.ambient = 0.28f;
        sl::set_light_3d(light);

        // Missiles, bombs and explosions light the voxels around them.
        {
            std::vector<sl::LocalLight3D> lights;
            for (const Shot &sh : g.shots)
            {
                sl::LocalLight3D l;
                l.position = {sh.x, sh.y, 6.0f};
                l.colour = sh.player ? glm::vec3(0.4f, 0.9f, 1.0f) : glm::vec3(1.0f, 0.35f, 0.25f);
                l.intensity = 2.2f;
                l.range = 18.0f;
                lights.push_back(l);
            }
            for (const Glow &gl : g.glows)
                if (gl.life >= 0.5f && gl.size1 > 3.0f && lights.size() < sl::max_local_lights_3d)
                {
                    sl::LocalLight3D l;
                    l.position = gl.pos + glm::vec3(0, 0, 5.0f);
                    l.colour = {gl.colour.red / 255.0f, gl.colour.green / 255.0f, gl.colour.blue / 255.0f};
                    l.intensity = 3.0f * (1.0f - gl.age / gl.life);
                    l.range = 30.0f;
                    lights.push_back(l);
                }
            if (lights.size() > sl::max_local_lights_3d)
                lights.resize(sl::max_local_lights_3d);
            sl::set_local_lights_3d(lights);
        }

        // Opaque: neon floor grid and floor line.
        {
            sl::Material m;
            m.lit = false;
            m.fog = false;
            m.tint = {90, 50, 230};
            std::vector<sl::InstanceData3D> lines;
            for (std::size_t i = 0; i < r.grid_boxes_pos.size(); ++i)
                draw_voxel_box(lines, r.grid_boxes_pos[i], r.grid_boxes_size[i]);
            sl::draw_mesh_instanced(r.cube, lines, m);
            std::vector<sl::InstanceData3D> edge;
            draw_voxel_box(edge, {0.0f, floor_y - 0.1f, 0.0f}, {2.0f * field + 6.0f, 0.3f, 1.6f});
            m.tint = {80, 255, 255};
            sl::draw_mesh_instanced(r.cube, edge, m);
        }

        // Opaque voxels, one batch per colour.
        std::array<std::vector<sl::InstanceData3D>, palette_count> solid;
        const glm::vec3 vsize(V * 0.92f, V * 0.92f, V * 3.2f);
        auto add_sprite = [&](const Sprite &s, int frame, glm::vec3 centre, float scale = 1.0f, float yaw = 0.0f) {
            for (const glm::vec3 &o : voxel_offsets(s, frame))
            {
                sl::InstanceData3D inst;
                inst.model = glm::rotate(glm::translate(glm::mat4(1.0f), centre), yaw, glm::vec3(0, 1, 0));
                inst.model = glm::scale(glm::translate(inst.model, o * scale), vsize * scale);
                solid[s.colour].push_back(inst);
            }
        };
        for (int rr = 0; rr < rows; ++rr)
            for (int c = 0; c < cols; ++c)
                if (g.alive[rr][c])
                    add_sprite(sprites[sprite_for_row(rr)], g.steps & 1, {invader_x(g, c), invader_y(g, rr), 0.0f}, 1.0f,
                               0.45f * std::sin(g.time * 1.6f + c * 0.5f + rr * 0.9f));
        const bool cannon_visible = g.state == State::playing || g.state == State::title;
        if (cannon_visible && (g.invuln <= 0 || static_cast<int>(g.invuln * 10.0f) % 2 == 0))
            add_sprite(cannon_sprite, 0, {g.player_x, player_y, 0.0f}, 1.0f, 0.3f * std::sin(g.time * 1.1f));
        if (g.ufo_on)
            add_sprite(ufo_sprite, 0, {g.ufo_x, 69.0f, 0.0f}, 1.0f, g.time * 2.5f);
        for (const Shield &sh : g.shields)
            for (int j = 0; j < Shield::h; ++j)
                for (int i = 0; i < Shield::w; ++i)
                    if (sh.v[j][i])
                        draw_voxel_box(solid[shield_c], sh.position(i, j), vsize);
        for (int i = 0; i < std::max(g.lives - 1, 0); ++i)
            add_sprite(cannon_sprite, 0, {-field + 15.0f + i * 7.5f, -3.4f, 0.0f}, 0.7f);

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
                sl::draw_mesh_instanced(r.cube, solid[c], material_for(c, 0.6f));

        // Debris fades by shrinking and becoming transparent.
        std::array<std::vector<sl::InstanceData3D>, palette_count> fading;
        for (const sl::RigidBox3D &b : g.debris.bodies())
        {
            const float life = b.lifetime > 0 ? 1.0f - b.age / b.lifetime : 1.0f;
            const float f = std::clamp(life * 1.8f, 0.0f, 1.0f);
            sl::InstanceData3D inst;
            glm::mat4 model = sl::RigidBoxWorld3D::model_matrix(b);
            model = glm::scale(model, glm::vec3(0.5f + 0.5f * f));
            inst.model = model;
            inst.tint = {255, 255, 255, static_cast<std::uint8_t>(255.0f * f)};
            fading[b.user % palette_count].push_back(inst);
        }
        for (int c = 0; c < palette_count; ++c)
            if (!fading[c].empty())
            {
                sl::Material m = material_for(c, 1.0f);
                m.blend = sl::MaterialBlend::alpha;
                sl::draw_mesh_instanced(r.cube, fading[c], m);
            }

        // Additive glows, back to front: stars, halos, then shots and explosions.
        for (const Renderer::Star &s : r.stars)
            draw_glow(r, s.pos, {s.size * 2.0f, s.size * 2.0f}, s.colour, 0.5f + 0.5f * std::sin(g.time * 1.5f + s.phase));
        for (int rr = 0; rr < rows; ++rr)
            for (int c = 0; c < cols; ++c)
                if (g.alive[rr][c])
                {
                    const Sprite &sp = sprites[sprite_for_row(rr)];
                    draw_glow(r, {invader_x(g, c), invader_y(g, rr), -1.2f}, {sp.w * V * 0.95f, sp.h * V * 0.95f}, palette[sp.colour], 0.22f);
                }
        if (cannon_visible)
            draw_glow(r, {g.player_x, player_y, -1.2f}, {5.0f, 3.0f}, palette[player_c], 0.22f);
        for (const Shield &sh : g.shields)
            draw_glow(r, {sh.x, sh.y, -1.2f}, {Shield::w * V * 0.62f, Shield::h * V * 0.8f}, palette[shield_c], 0.1f);
        if (g.ufo_on)
            draw_glow(r, {g.ufo_x, 69.0f, -1.2f}, {7.0f, 3.2f}, palette[ufo_c], 0.3f);

        for (const Shot &s : g.shots)
        {
            if (s.player)
            {
                draw_glow(r, {s.x, s.y, 0.2f}, {0.9f, 3.4f}, {120, 230, 255}, 0.95f);
                draw_glow(r, {s.x, s.y, 0.3f}, {0.35f, 2.4f}, {255, 255, 255}, 1.0f);
                draw_glow(r, {s.x, s.y + 1.2f, 0.3f}, {2.4f, 2.4f}, {160, 240, 255}, 0.6f);
            }
            else
            {
                for (int k = 0; k < 4; ++k)
                    draw_glow(r, {s.x + std::sin(s.phase * 18.0f + k * 1.2f) * 0.5f, s.y + k * 0.7f, 0.2f}, {0.9f, 0.9f}, {255, 90, 70}, 0.95f);
                draw_glow(r, {s.x, s.y, 0.3f}, {2.6f, 2.6f}, {255, 140, 90}, 0.5f);
            }
        }
        for (const Glow &gl : g.glows)
        {
            const float t = gl.age / gl.life;
            const float size = gl.size0 + (gl.size1 - gl.size0) * t;
            const float alpha = gl.strength * (1.0f - t) * (1.0f - t);
            draw_glow(r, gl.pos, {size, size}, gl.colour, alpha);
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
        sl::gprintf(24, window_h - 30, cyan, "LIVES");
        if (g.message_time > 0 && !g.message.empty())
            sl::gprintf_center(300, lime, "%s", g.message.c_str());
        if (g.state == State::title)
        {
            sl::gprintf_center(220, cyan, "E X I N V A D E R S");
            sl::gprintf_center(260, white, "Press ENTER or SPACE to play");
            sl::gprintf_center(284, white, "Left / Right move, Space fires");
        }
        else if (g.state == State::game_over)
        {
            sl::gprintf_center(250, pink, "G A M E   O V E R");
            sl::gprintf_center(280, white, "Press ENTER to play again");
        }
        else if (g.state == State::wave_clear)
            sl::gprintf_center(280, lime, "WAVE CLEARED");
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
    sl::set_window_title("exinvaders v" EXINVADERS_VERSION);

    Renderer r;
    r.surface = sl::create_surface_3d(window_w, window_h, 4);
    r.cube = sl::create_cube_mesh(1.0f);
    r.quad = make_quad();
    r.puff = make_puff();
    if (!r.surface || !r.cube || !r.quad || !r.puff || !r.bloom.initialise())
    {
        std::fprintf(stderr, "exinvaders needs 3D surfaces and bloom, which this backend doesn't provide.\n");
        sl::shutdown();
        return -1;
    }
    r.bloom.set_threshold(0.5f);
    r.bloom.set_intensity(1.3f);
    r.bloom.set_radius(3.0f);
    build_scenery(r);

    static Game g;
    std::srand(7);
    init_audio(g.audio);
    g.debris.floor_height = floor_y + V * 0.4f;
    g.debris.gravity = {0.0f, -26.0f, 0.0f};
    g.debris.max_bodies = 3000;
    start_wave(g); // the formation shown behind the title screen
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
    shutdown_audio(g.audio);
    r.bloom.shutdown();
    sl::destroy_bitmap(r.puff);
    sl::destroy_mesh(r.quad);
    sl::destroy_mesh(r.cube);
    sl::destroy_surface_3d(r.surface);
    sl::shutdown();
    return 0;
}
