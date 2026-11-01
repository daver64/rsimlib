// exbomberman: a neon Bomberman-inspired arena. Move through the grid, place bombs,
// and carve voxel-by-voxel through destructible 8x8x8 walls.
//
// Arrows or WASD move, Space or Z places a bomb, Enter starts/restarts, Escape quits.
// Options: --autoplay, --shot FILE and --shot-at SECONDS.
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
    constexpr int grid_w = 13, grid_h = 11;
    constexpr int voxel_side = 8;
    constexpr int voxel_count = voxel_side * voxel_side * voxel_side;
    constexpr float cell_size = 3.2f;
    constexpr float voxel_size = cell_size / voxel_side;
    constexpr float wall_footprint = 0.72f;
    constexpr float wall_half_extent = voxel_size * (voxel_side - 1) * wall_footprint * 0.5f + voxel_size * wall_footprint * 0.4f;
    constexpr float board_w = grid_w * cell_size, board_h = grid_h * cell_size;
    constexpr float pi = 3.14159265f;

    enum Palette
    {
        cyan_c,
        pink_c,
        orange_c,
        lime_c,
        yellow_c,
        violet_c,
        white_c,
        player_c,
        player_trim_c,
        palette_count
    };
    const sl::Colour palette[palette_count] = {{25, 230, 255}, {255, 35, 205}, {255, 125, 25},
                                               {110, 255, 65}, {255, 230, 45}, {150, 75, 255},
                                               {240, 250, 255}, {180, 255, 30}, {255, 60, 170}};

    float random_unit() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
    float between(float a, float b) { return a + (b - a) * random_unit(); }

    enum class TileKind : std::uint8_t
    {
        empty,
        hard,
        soft
    };
    struct Tile
    {
        TileKind kind = TileKind::empty;
        Palette colour = pink_c;
    };
    struct Bomb
    {
        int x = 0, z = 0, range = 2;
        float fuse = 1.6f;
        bool owner_can_leave = true;
    };
    struct Flame
    {
        int x = 0, z = 0;
        float life = 0.5f;
    };
    struct Enemy
    {
        int x = 0, z = 0;
        float move_timer = 0.0f;
        bool alive = true;
    };
    enum class PowerupKind
    {
        range,
        bomb
    };
    struct Powerup
    {
        int x = 0, z = 0;
        PowerupKind kind = PowerupKind::range;
        float phase = 0.0f;
    };
    struct VoxelDebris
    {
        glm::vec3 pos, velocity;
        Palette colour = pink_c;
        float age = 0.0f, life = 1.0f;
        float delay = 0.0f;
    };
    struct Audio
    {
        sl::Sample *plant = nullptr, *blast = nullptr, *pickup = nullptr;
        bool ready = false;
    };
    enum class State
    {
        title,
        playing,
        wave_clear,
        game_over
    };
    struct Game
    {
        State state = State::title;
        std::array<Tile, grid_w * grid_h> tiles{};
        glm::vec2 player{0.0f};
        std::vector<Bomb> bombs;
        std::vector<Flame> flames;
        std::vector<Enemy> enemies;
        std::vector<Powerup> powerups;
        std::vector<VoxelDebris> debris;
        int score = 0, high_score = 0, lives = 3, wave = 1;
        int bomb_range = 2, max_bombs = 1;
        float bomb_cooldown = 0.0f, invulnerable = 0.0f, state_timer = 0.0f, time = 0.0f;
        float shake_strength = 0.0f, shake_time = 0.0f, shake_duration = 0.0f;
        Audio audio;
    };
    struct Input
    {
        bool left = false, right = false, up = false, down = false, bomb = false;
    };
    struct Renderer
    {
        sl::Surface3D *surface = nullptr;
        sl::Mesh *cube = nullptr;
        sl::Bloom bloom;
    };

    std::size_t tile_index(int x, int z) { return static_cast<std::size_t>(z * grid_w + x); }
    Tile &tile_at(Game &g, int x, int z) { return g.tiles[tile_index(x, z)]; }
    const Tile &tile_at(const Game &g, int x, int z) { return g.tiles[tile_index(x, z)]; }
    bool inside(int x, int z) { return x >= 0 && x < grid_w && z >= 0 && z < grid_h; }
    glm::vec2 cell_world(int x, int z)
    {
        return {(x + 0.5f) * cell_size - board_w * 0.5f, (z + 0.5f) * cell_size - board_h * 0.5f};
    }
    void player_cell(const Game &g, int &x, int &z)
    {
        x = static_cast<int>(std::floor((g.player.x + board_w * 0.5f) / cell_size));
        z = static_cast<int>(std::floor((g.player.y + board_h * 0.5f) / cell_size));
        x = std::clamp(x, 0, grid_w - 1);
        z = std::clamp(z, 0, grid_h - 1);
    }

    sl::Sample *synth(float duration, sl::Adsr envelope, std::vector<sl::SynthOscillator> oscillators)
    {
        sl::SynthParams params;
        params.duration = duration;
        params.envelope = envelope;
        params.oscillators = std::move(oscillators);
        return sl::generate_sample(params);
    }
    void init_audio(Audio &audio)
    {
        if (!sl::audio_fx_init())
            return;
        using W = sl::Waveform;
        audio.plant = synth(0.13f, {0.001f, 0.04f, 0.25f, 0.05f}, {{W::square, 520.0f, 0.35f, {1.0f, 0.2f}}});
        audio.blast = synth(0.55f, {0.002f, 0.18f, 0.25f, 0.25f},
                            {{W::noise, 1.0f, 0.7f, {}}, {W::saw, 100.0f, 0.45f, {1.0f, 0.08f}}});
        audio.pickup = synth(0.32f, {0.002f, 0.08f, 0.6f, 0.16f},
                             {{W::square, 780.0f, 0.35f, {1.0f, 1.0f}}, {W::triangle, 1170.0f, 0.25f, {1.0f, 1.2f}}});
        audio.ready = audio.plant && audio.blast && audio.pickup;
    }
    void play(const Audio &audio, sl::Sample *sample, int volume = 180)
    {
        if (audio.ready && sample)
            sl::play_sample(sample, volume, 128, 1000);
    }

    bool bomb_at(const Game &g, int x, int z)
    {
        return std::any_of(g.bombs.begin(), g.bombs.end(), [&](const Bomb &b) { return b.x == x && b.z == z; });
    }
    bool circle_overlaps_box(glm::vec2 point, glm::vec2 centre, float radius, glm::vec2 half_extent)
    {
        const glm::vec2 closest{std::clamp(point.x, centre.x - half_extent.x, centre.x + half_extent.x),
                                std::clamp(point.y, centre.y - half_extent.y, centre.y + half_extent.y)};
        const glm::vec2 delta = point - closest;
        return glm::dot(delta, delta) < radius * radius;
    }
    bool passable(const Game &g, int x, int z, bool player_can_leave_bomb = false)
    {
        if (!inside(x, z) || tile_at(g, x, z).kind != TileKind::empty)
            return false;
        if (!bomb_at(g, x, z))
            return true;
        if (player_can_leave_bomb)
        {
            const auto bomb = std::find_if(g.bombs.begin(), g.bombs.end(),
                                           [&](const Bomb &b) { return b.x == x && b.z == z; });
            return bomb != g.bombs.end() && bomb->owner_can_leave;
        }
        return false;
    }
    bool passable_world(const Game &g, glm::vec2 p)
    {
        constexpr float player_radius = 0.34f;
        if (p.x < -board_w * 0.5f + player_radius || p.x > board_w * 0.5f - player_radius ||
            p.y < -board_h * 0.5f + player_radius || p.y > board_h * 0.5f - player_radius)
            return false;
        for (int z = 0; z < grid_h; ++z)
            for (int x = 0; x < grid_w; ++x)
            {
                if (tile_at(g, x, z).kind == TileKind::empty)
                    continue;
                if (circle_overlaps_box(p, cell_world(x, z), player_radius,
                                        {wall_half_extent, wall_half_extent}))
                    return false;
            }

        int current_x = 0, current_z = 0;
        player_cell(g, current_x, current_z);
        for (const Bomb &bomb : g.bombs)
        {
            if (bomb.owner_can_leave && current_x == bomb.x && current_z == bomb.z)
                continue;
            if (circle_overlaps_box(p, cell_world(bomb.x, bomb.z), player_radius, {0.53f, 0.53f}))
                return false;
        }

        constexpr float enemy_radius = 0.56f;
        for (const Enemy &enemy : g.enemies)
        {
            if (!enemy.alive)
                continue;
            const glm::vec2 enemy_pos = cell_world(enemy.x, enemy.z);
            const glm::vec2 delta = p - enemy_pos;
            const float min_distance = player_radius + enemy_radius;
            if (glm::dot(delta, delta) < min_distance * min_distance)
                return false;
        }
        return true;
    }
    bool enemy_at(const Game &g, int x, int z, const Enemy *except = nullptr)
    {
        return std::any_of(g.enemies.begin(), g.enemies.end(), [&](const Enemy &e) {
            return e.alive && &e != except && e.x == x && e.z == z;
        });
    }
    bool spawn_safe_cell(int x, int z)
    {
        constexpr int safe[][2] = {{1, 1}, {11, 9}, {11, 1}, {1, 9}, {9, 9}, {9, 1}, {3, 9}, {7, 1}};
        for (const auto &p : safe)
            if (std::abs(p[0] - x) + std::abs(p[1] - z) <= 1)
                return true;
        return false;
    }
    void build_arena(Game &g)
    {
        for (int z = 0; z < grid_h; ++z)
            for (int x = 0; x < grid_w; ++x)
            {
                Tile &t = tile_at(g, x, z);
                t = {};
                if (x == 0 || z == 0 || x == grid_w - 1 || z == grid_h - 1 || (x % 2 == 0 && z % 2 == 0))
                {
                    t.kind = TileKind::hard;
                    t.colour = (x + z) % 4 == 0 ? violet_c : cyan_c;
                }
                else if (!spawn_safe_cell(x, z) && random_unit() < 0.56f)
                {
                    t.kind = TileKind::soft;
                    t.colour = (std::rand() % 3 == 0) ? orange_c : pink_c;
                }
            }
    }
    void start_wave(Game &g)
    {
        build_arena(g);
        g.bombs.clear();
        g.flames.clear();
        g.powerups.clear();
        g.enemies.clear();
        g.debris.clear();
        g.player = cell_world(1, 1);
        constexpr int spawn[][2] = {{11, 9}, {11, 1}, {1, 9}, {9, 9}, {9, 1}, {3, 9}, {7, 1}, {11, 5}};
        const int count = std::min(3 + g.wave, static_cast<int>(std::size(spawn)));
        for (int i = 0; i < count; ++i)
        {
            g.enemies.push_back({spawn[i][0], spawn[i][1], between(0.1f, 0.8f), true});
            tile_at(g, spawn[i][0], spawn[i][1]).kind = TileKind::empty;
        }
        g.bomb_cooldown = 0.0f;
        g.invulnerable = 1.7f;
        g.state = State::playing;
    }
    void new_game(Game &g)
    {
        g.score = 0;
        g.lives = 3;
        g.wave = 1;
        g.bomb_range = 2;
        g.max_bombs = 1;
        start_wave(g);
    }

    bool flame_at(const Game &g, int x, int z)
    {
        return std::any_of(g.flames.begin(), g.flames.end(), [&](const Flame &f) { return f.x == x && f.z == z; });
    }
    void start_shake(Game &g, float strength, float duration)
    {
        const float current = g.shake_duration > 0.0f
                                  ? g.shake_strength * g.shake_time / g.shake_duration
                                  : 0.0f;
        if (strength > current)
        {
            g.shake_strength = strength;
            g.shake_time = duration;
            g.shake_duration = duration;
        }
    }
    void damage_player(Game &g)
    {
        if (g.invulnerable > 0 || g.state != State::playing)
            return;
        start_shake(g, 3.0f, 0.58f);
        --g.lives;
        if (g.lives <= 0)
        {
            g.state = State::game_over;
            g.high_score = std::max(g.high_score, g.score);
            return;
        }
        g.player = cell_world(1, 1);
        g.invulnerable = 2.0f;
    }
    void add_flame(Game &g, int x, int z)
    {
        if (!inside(x, z))
            return;
        auto it = std::find_if(g.flames.begin(), g.flames.end(), [&](const Flame &f) { return f.x == x && f.z == z; });
        if (it == g.flames.end())
            g.flames.push_back({x, z, 0.55f});
        else
            it->life = std::max(it->life, 0.55f);

        int px = 0, pz = 0;
        player_cell(g, px, pz);
        if (px == x && pz == z)
            damage_player(g);
        for (Enemy &enemy : g.enemies)
            if (enemy.alive && enemy.x == x && enemy.z == z)
            {
                enemy.alive = false;
                g.score += 250;
                g.high_score = std::max(g.high_score, g.score);
            }
    }
    void destroy_block(Game &g, int x, int z)
    {
        Tile &block = tile_at(g, x, z);
        if (block.kind != TileKind::soft)
            return;
        const Palette colour = block.colour;
        block.kind = TileKind::empty;
        const glm::vec2 centre = cell_world(x, z);
        const std::size_t remaining = g.debris.size() < 9000 ? 9000 - g.debris.size() : 0;
        for (int i = 0; i < voxel_count && static_cast<std::size_t>(i) < remaining; ++i)
        {
            const int vx = i % voxel_side;
            const int vz = (i / voxel_side) % voxel_side;
            const int vy = i / (voxel_side * voxel_side);
            const glm::vec3 pos{centre.x + (vx - (voxel_side - 1) * 0.5f) * voxel_size,
                                (vy + 0.5f) * voxel_size,
                                centre.y + (vz - (voxel_side - 1) * 0.5f) * voxel_size};
            glm::vec3 direction{pos.x - centre.x, between(0.2f, 2.0f), pos.z - centre.y};
            if (glm::length(direction) < 0.01f)
                direction = {between(-1.0f, 1.0f), 1.0f, between(-1.0f, 1.0f)};
            direction = glm::normalize(direction);
            const float delay = (vx + vy + vz) / (3.0f * (voxel_side - 1)) * 0.32f;
            g.debris.push_back({pos, direction * between(2.0f, 7.0f), colour, 0.0f, between(0.75f, 1.1f), delay});
        }
        g.score += 25;
        g.high_score = std::max(g.high_score, g.score);
        if (std::rand() % 100 < 24)
            g.powerups.push_back({x, z, std::rand() % 2 == 0 ? PowerupKind::range : PowerupKind::bomb, 0.0f});
    }
    void detonate(Game &g, const Bomb &bomb)
    {
        start_shake(g, 0.55f, 0.2f);
        play(g.audio, g.audio.blast, 205);
        add_flame(g, bomb.x, bomb.z);
        constexpr int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto &dir : dirs)
            for (int step = 1; step <= bomb.range; ++step)
            {
                const int x = bomb.x + dir[0] * step, z = bomb.z + dir[1] * step;
                if (!inside(x, z) || tile_at(g, x, z).kind == TileKind::hard)
                    break;
                add_flame(g, x, z);
                if (bomb_at(g, x, z))
                    for (Bomb &other : g.bombs)
                        if (other.x == x && other.z == z)
                            other.fuse = 0.0f;
                if (tile_at(g, x, z).kind == TileKind::soft)
                {
                    destroy_block(g, x, z);
                    break;
                }
            }
    }
    void place_bomb(Game &g)
    {
        if (g.bombs.size() >= static_cast<std::size_t>(g.max_bombs) || g.bomb_cooldown > 0.0f)
            return;
        int x = 0, z = 0;
        player_cell(g, x, z);
        if (bomb_at(g, x, z))
            return;
        g.bombs.push_back({x, z, g.bomb_range, 1.55f, true});
        g.bomb_cooldown = 0.25f;
        play(g.audio, g.audio.plant, 145);
    }
    void update_enemies(Game &g, float dt)
    {
        int px = 0, pz = 0;
        player_cell(g, px, pz);
        for (Enemy &enemy : g.enemies)
        {
            if (!enemy.alive)
                continue;
            enemy.move_timer -= dt;
            if (enemy.move_timer > 0.0f)
                continue;
            enemy.move_timer = std::max(0.32f, 0.82f - g.wave * 0.04f);
            std::array<glm::ivec2, 4> moves = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
            std::sort(moves.begin(), moves.end(), [&](glm::ivec2 a, glm::ivec2 b) {
                const int da = std::abs(enemy.x + a.x - px) + std::abs(enemy.z + a.y - pz);
                const int db = std::abs(enemy.x + b.x - px) + std::abs(enemy.z + b.y - pz);
                return da < db;
            });
            for (glm::ivec2 move : moves)
            {
                const int nx = enemy.x + move.x, nz = enemy.z + move.y;
                if (passable(g, nx, nz) && !enemy_at(g, nx, nz, &enemy) && !flame_at(g, nx, nz))
                {
                    enemy.x = nx;
                    enemy.z = nz;
                    break;
                }
            }
            if (enemy.x == px && enemy.z == pz)
                damage_player(g);
        }
    }
    void update(Game &g, float dt, const Input &input)
    {
        g.time += dt;
        g.shake_time = std::max(0.0f, g.shake_time - dt);
        g.bomb_cooldown = std::max(0.0f, g.bomb_cooldown - dt);
        g.invulnerable = std::max(0.0f, g.invulnerable - dt);
        for (VoxelDebris &d : g.debris)
        {
            d.age += dt;
            if (d.age >= d.delay)
            {
                d.pos += d.velocity * dt;
                d.velocity.y -= 9.0f * dt;
                d.velocity *= std::max(0.0f, 1.0f - 0.8f * dt);
            }
        }
        g.debris.erase(
            std::remove_if(g.debris.begin(), g.debris.end(), [](const VoxelDebris &d) { return d.age >= d.delay + d.life; }),
            g.debris.end());
        for (Flame &flame : g.flames)
            flame.life -= dt;
        g.flames.erase(std::remove_if(g.flames.begin(), g.flames.end(), [](const Flame &f) { return f.life <= 0.0f; }),
                       g.flames.end());
        for (Powerup &powerup : g.powerups)
            powerup.phase += dt * 3.0f;

        if (g.state == State::title || g.state == State::game_over)
            return;
        if (g.state == State::wave_clear)
        {
            g.state_timer -= dt;
            if (g.state_timer <= 0.0f)
            {
                ++g.wave;
                start_wave(g);
            }
            return;
        }

        glm::vec2 move{(input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f),
                       (input.down ? 1.0f : 0.0f) - (input.up ? 1.0f : 0.0f)};
        if (glm::length(move) > 0.0f)
            move = glm::normalize(move) * 6.4f * dt;
        glm::vec2 next = g.player;
        next.x += move.x;
        if (passable_world(g, next))
            g.player.x = next.x;
        next = g.player;
        next.y += move.y;
        if (passable_world(g, next))
            g.player.y = next.y;
        for (Bomb &bomb : g.bombs)
        {
            const glm::vec2 bomb_centre = cell_world(bomb.x, bomb.z);
            if (bomb.owner_can_leave &&
                (std::fabs(g.player.x - bomb_centre.x) > cell_size * 0.5f + 0.34f ||
                 std::fabs(g.player.y - bomb_centre.y) > cell_size * 0.5f + 0.34f))
                bomb.owner_can_leave = false;
        }
        if (input.bomb)
            place_bomb(g);

        for (Bomb &bomb : g.bombs)
            bomb.fuse -= dt;
        while (true)
        {
            const auto due = std::find_if(g.bombs.begin(), g.bombs.end(), [](const Bomb &b) { return b.fuse <= 0.0f; });
            if (due == g.bombs.end())
                break;
            const Bomb bomb = *due;
            g.bombs.erase(due);
            detonate(g, bomb);
        }

        update_enemies(g, dt);
        int px = 0, pz = 0;
        player_cell(g, px, pz);
        for (auto it = g.powerups.begin(); it != g.powerups.end();)
        {
            if (it->x == px && it->z == pz)
            {
                if (it->kind == PowerupKind::range)
                    ++g.bomb_range;
                else
                    ++g.max_bombs;
                g.score += 100;
                g.high_score = std::max(g.high_score, g.score);
                play(g.audio, g.audio.pickup);
                it = g.powerups.erase(it);
            }
            else
                ++it;
        }
        if (std::none_of(g.enemies.begin(), g.enemies.end(), [](const Enemy &e) { return e.alive; }))
        {
            g.state = State::wave_clear;
            g.state_timer = 2.0f;
        }
    }

    void push_box(std::vector<sl::InstanceData3D> &out, glm::vec3 centre, glm::vec3 size,
                  sl::Colour tint = {255, 255, 255, 255})
    {
        sl::InstanceData3D inst;
        inst.model = glm::scale(glm::translate(glm::mat4(1.0f), centre), size);
        inst.tint = tint;
        out.push_back(inst);
    }
    sl::Material material_for(Palette colour, float emission = 1.3f)
    {
        sl::Material m;
        m.lit = true;
        m.fog = false;
        m.tint = palette[colour];
        m.emissive[0] = palette[colour].red / 255.0f * emission;
        m.emissive[1] = palette[colour].green / 255.0f * emission;
        m.emissive[2] = palette[colour].blue / 255.0f * emission;
        return m;
    }
    void draw_scene(Renderer &r, const Game &g)
    {
        const float shake = g.shake_duration > 0.0f ? g.shake_strength * g.shake_time / g.shake_duration : 0.0f;
        const glm::vec2 shake_offset{std::sin(g.time * 83.0f) * shake, std::cos(g.time * 97.0f) * shake};
        sl::Camera camera;
        camera.position = {shake_offset.x, 54.0f, 24.0f + shake_offset.y};
        camera.target = {shake_offset.x, 0.0f, shake_offset.y};
        camera.field_of_view = 43.0f;
        camera.near_plane = 1.0f;
        camera.far_plane = 220.0f;
        if (!sl::begin_surface_3d(r.surface, camera))
            return;
        sl::clear_surface_3d({5, 2, 20});
        sl::Light3D light;
        light.direction = {-0.45f, -0.85f, -0.35f};
        light.ambient = 0.42f;
        sl::set_light_3d(light);

        sl::Material floor_material;
        floor_material.lit = false;
        floor_material.fog = false;
        floor_material.tint = {12, 8, 28};
        glm::mat4 floor_model = glm::translate(glm::mat4(1.0f), {0.0f, -0.24f, 0.0f});
        floor_model = glm::scale(floor_model, {board_w + 1.0f, 0.4f, board_h + 1.0f});
        sl::draw_mesh(r.cube, floor_model, floor_material);

        std::vector<sl::InstanceData3D> grid_lines;
        for (int x = 0; x <= grid_w; ++x)
            push_box(grid_lines, {-board_w * 0.5f + x * cell_size, -0.005f, 0.0f}, {0.035f, 0.025f, board_h});
        for (int z = 0; z <= grid_h; ++z)
            push_box(grid_lines, {0.0f, -0.005f, -board_h * 0.5f + z * cell_size}, {board_w, 0.025f, 0.035f});
        sl::Material grid_material = material_for(cyan_c, 0.8f);
        grid_material.tint = {40, 180, 210};
        sl::draw_mesh_instanced(r.cube, grid_lines, grid_material);

        std::array<std::vector<sl::InstanceData3D>, palette_count> voxels;
        for (int z = 0; z < grid_h; ++z)
            for (int x = 0; x < grid_w; ++x)
            {
                const Tile &tile = tile_at(g, x, z);
                if (tile.kind == TileKind::empty)
                    continue;
                const glm::vec2 centre = cell_world(x, z);
                auto &batch = voxels[tile.colour];
                for (int vy = 0; vy < voxel_side; ++vy)
                    for (int vz = 0; vz < voxel_side; ++vz)
                        for (int vx = 0; vx < voxel_side; ++vx)
                        {
                            const glm::vec3 pos{centre.x + (vx - (voxel_side - 1) * 0.5f) * voxel_size * wall_footprint,
                                                (vy + 0.5f) * voxel_size,
                                                centre.y + (vz - (voxel_side - 1) * 0.5f) * voxel_size * wall_footprint};
                            push_box(batch, pos, {voxel_size * wall_footprint * 0.82f, voxel_size * 0.96f,
                                                  voxel_size * wall_footprint * 0.82f});
                        }
            }
        for (int c = 0; c < palette_count; ++c)
            if (!voxels[c].empty())
                sl::draw_mesh_instanced(r.cube, voxels[c], material_for(static_cast<Palette>(c), 2.0f));

        std::vector<sl::InstanceData3D> bombs, bomb_glow, flames, flame_glow, pickups, player, player_details,
            player_beacon, enemies;
        for (const Bomb &bomb : g.bombs)
        {
            const glm::vec2 p = cell_world(bomb.x, bomb.z);
            const float pulse = 0.84f + 0.16f * std::sin(g.time * 12.0f);
            push_box(bombs, {p.x, 0.52f, p.y}, {1.05f * pulse, 1.05f * pulse, 1.05f * pulse});
            push_box(bombs, {p.x, 1.12f, p.y}, {0.26f, 0.24f, 0.26f}, {255, 220, 100, 255});
            push_box(bomb_glow, {p.x, 0.12f, p.y}, {1.8f * pulse, 0.12f, 1.8f * pulse});
        }
        for (const Flame &flame : g.flames)
        {
            const glm::vec2 p = cell_world(flame.x, flame.z);
            const auto alpha = static_cast<std::uint8_t>(255.0f * std::clamp(flame.life / 0.55f, 0.0f, 1.0f));
            push_box(flames, {p.x, 0.26f, p.y}, {cell_size * 0.92f, 0.32f, cell_size * 0.92f},
                     {255, 210, 115, alpha});
            push_box(flame_glow, {p.x, 0.08f, p.y}, {cell_size * 1.3f, 0.1f, cell_size * 1.3f},
                     {255, 65, 12, static_cast<std::uint8_t>(alpha * 0.55f)});
        }
        for (const Powerup &powerup : g.powerups)
        {
            const glm::vec2 p = cell_world(powerup.x, powerup.z);
            const float y = 0.72f + 0.18f * std::sin(powerup.phase);
            const Palette colour = powerup.kind == PowerupKind::range ? yellow_c : lime_c;
            push_box(pickups, {p.x, y, p.y}, {0.72f, 0.72f, 0.72f}, palette[colour]);
        }
        push_box(player, {g.player.x, 0.75f, g.player.y}, {1.05f, 1.3f, 0.9f}, palette[player_c]);
        push_box(player, {g.player.x, 1.62f, g.player.y}, {0.78f, 0.72f, 0.78f}, palette[white_c]);
        push_box(player_details, {g.player.x, 1.65f, g.player.y + 0.4f}, {0.48f, 0.24f, 0.16f},
                 palette[player_trim_c]);
        push_box(player_details, {g.player.x - 0.3f, 0.14f, g.player.y}, {0.34f, 0.26f, 0.48f},
                 palette[player_trim_c]);
        push_box(player_details, {g.player.x + 0.3f, 0.14f, g.player.y}, {0.34f, 0.26f, 0.48f},
                 palette[player_trim_c]);
        push_box(player_details, {g.player.x, 3.2f, g.player.y}, {0.18f, 2.8f, 0.18f},
                 palette[player_trim_c]);
        push_box(player_beacon, {g.player.x, 4.95f + 0.18f * std::sin(g.time * 6.0f), g.player.y},
                 {1.0f, 1.0f, 1.0f});
        const float marker = 1.35f;
        push_box(player_details, {g.player.x, 0.06f, g.player.y - marker}, {marker * 2.0f, 0.08f, 0.09f},
                 palette[player_trim_c]);
        push_box(player_details, {g.player.x, 0.06f, g.player.y + marker}, {marker * 2.0f, 0.08f, 0.09f},
                 palette[player_trim_c]);
        push_box(player_details, {g.player.x - marker, 0.06f, g.player.y}, {0.09f, 0.08f, marker * 2.0f},
                 palette[player_trim_c]);
        push_box(player_details, {g.player.x + marker, 0.06f, g.player.y}, {0.09f, 0.08f, marker * 2.0f},
                 palette[player_trim_c]);
        for (const Enemy &enemy : g.enemies)
            if (enemy.alive)
            {
                const glm::vec2 p = cell_world(enemy.x, enemy.z);
                push_box(enemies, {p.x, 0.62f, p.y}, {1.12f, 1.12f, 1.12f}, palette[pink_c]);
                push_box(enemies, {p.x - 0.32f, 1.05f, p.y - 0.35f}, {0.16f, 0.18f, 0.16f}, palette[white_c]);
                push_box(enemies, {p.x + 0.32f, 1.05f, p.y - 0.35f}, {0.16f, 0.18f, 0.16f}, palette[white_c]);
            }
        if (!bombs.empty())
            sl::draw_mesh_instanced(r.cube, bombs, material_for(orange_c, 1.8f));
        if (!bomb_glow.empty())
        {
            sl::Material glow = material_for(orange_c, 3.5f);
            glow.blend = sl::MaterialBlend::additive;
            glow.tint.alpha = 100;
            sl::draw_mesh_instanced(r.cube, bomb_glow, glow);
        }
        if (!flames.empty())
        {
            sl::Material m = material_for(yellow_c, 2.8f);
            m.blend = sl::MaterialBlend::alpha;
            sl::draw_mesh_instanced(r.cube, flames, m);
        }
        if (!flame_glow.empty())
        {
            sl::Material glow = material_for(orange_c, 3.2f);
            glow.blend = sl::MaterialBlend::additive;
            sl::draw_mesh_instanced(r.cube, flame_glow, glow);
        }
        if (!pickups.empty())
            sl::draw_mesh_instanced(r.cube, pickups, material_for(lime_c, 1.8f));
        if (!player.empty())
            sl::draw_mesh_instanced(r.cube, player, material_for(player_c, 3.2f));
        if (!player_details.empty())
            sl::draw_mesh_instanced(r.cube, player_details, material_for(player_trim_c, 3.5f));
        if (!player_beacon.empty())
            sl::draw_mesh_instanced(r.cube, player_beacon, material_for(yellow_c, 4.0f));
        if (!enemies.empty())
            sl::draw_mesh_instanced(r.cube, enemies, material_for(pink_c, 1.5f));

        std::array<std::vector<sl::InstanceData3D>, palette_count> debris;
        for (const VoxelDebris &d : g.debris)
        {
            const bool waiting = d.age < d.delay;
            const float fade = waiting ? 1.0f : std::clamp(1.0f - (d.age - d.delay) / d.life, 0.0f, 1.0f);
            const auto alpha = static_cast<std::uint8_t>(255.0f * fade);
            push_box(debris[d.colour], d.pos, glm::vec3(voxel_size * (waiting ? 0.91f : 0.45f + fade * 0.45f)),
                     {255, 255, 255, alpha});
        }
        for (int c = 0; c < palette_count; ++c)
            if (!debris[c].empty())
            {
                sl::Material m = material_for(static_cast<Palette>(c), 1.4f);
                m.blend = sl::MaterialBlend::alpha;
                sl::draw_mesh_instanced(r.cube, debris[c], m);
            }
        sl::end_surface_3d();
        r.bloom.apply(sl::surface_3d_bitmap(r.surface), 0, 0, window_w, window_h);
    }

    void draw_hud(const Game &g)
    {
        const sl::Colour white{235, 240, 255}, cyan{70, 235, 255}, pink{255, 80, 220}, lime{150, 255, 75};
        sl::gprintf(24, 10, pink, "SCORE");
        sl::gprintf(24, 30, white, "%06d", g.score);
        sl::gprintf_center(10, pink, "HI-SCORE");
        sl::gprintf_center(30, white, "%06d", g.high_score);
        sl::gprintf(window_w - 175, 10, pink, "WAVE");
        sl::gprintf(window_w - 175, 30, white, "%d", g.wave);
        sl::gprintf(window_w - 175, 55, cyan, "LIVES %d", g.lives);
        sl::gprintf(24, window_h - 34, cyan, "ARROWS / WASD MOVE     SPACE / Z BOMB");
        if (g.state == State::title)
        {
            sl::gprintf_center(205, cyan, "E X B O M B E R M A N");
            sl::gprintf_center(250, white, "Place bombs, carve voxel walls, and outsmart the bots");
            sl::gprintf_center(280, white, "Press ENTER to start");
        }
        else if (g.state == State::game_over)
        {
            sl::gprintf_center(235, pink, "G A M E   O V E R");
            sl::gprintf_center(270, white, "Press ENTER to play again");
        }
        else if (g.state == State::wave_clear)
            sl::gprintf_center(250, lime, "ARENA CLEARED");
    }

    Input autoplay_input(const Game &g)
    {
        const int phase = static_cast<int>(g.time / 1.4f) % 4;
        Input input;
        input.right = phase == 0;
        input.down = phase == 1;
        input.left = phase == 2;
        input.up = phase == 3;
        input.bomb = std::fmod(g.time, 2.0f) < 0.04f;
        return input;
    }
} // namespace

int main(int argc, char *argv[])
{
    bool autoplay = false;
    std::string shot_path;
    float shot_at = 8.0f;
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
    sl::set_window_title("exbomberman v" EXBOMBERMAN_VERSION);

    Renderer renderer;
    renderer.surface = sl::create_surface_3d(window_w, window_h, 4);
    renderer.cube = sl::create_cube_mesh(1.0f);
    if (!renderer.surface || !renderer.cube || !renderer.bloom.initialise())
    {
        std::fprintf(stderr, "exbomberman needs 3D surfaces and bloom, which this backend doesn't provide.\n");
        sl::shutdown();
        return -1;
    }
    renderer.bloom.set_threshold(0.22f);
    renderer.bloom.set_intensity(2.4f);
    renderer.bloom.set_radius(4.2f);

    Game game;
    std::srand(31);
    init_audio(game.audio);
    start_wave(game);
    game.state = State::title;
    if (autoplay)
        new_game(game);

    sl::set_fps(60);
    bool running = true, saved = false, shot_failed = false;
    float last = static_cast<float>(sl::time_ms()) * 0.001f, elapsed = 0.0f;
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
                     (event.key() == sl::Event::Key::return_key || event.key() == sl::Event::Key::keypad_enter) &&
                     (game.state == State::title || game.state == State::game_over))
                new_game(game);
            sl::display_handle_event(event);
        }
        const float now = static_cast<float>(sl::time_ms()) * 0.001f;
        const float dt = std::clamp(now - last, 0.0f, 0.05f);
        last = now;
        elapsed += dt;

        Input input;
        if (autoplay)
        {
            game_over_frames = game.state == State::game_over ? game_over_frames + 1 : 0;
            if (game_over_frames > 120)
                new_game(game);
            input = autoplay_input(game);
        }
        else
        {
            input.left = sl::key_down(sl::Event::Key::arrow_left) || sl::key_down(sl::Event::Key::letter_a);
            input.right = sl::key_down(sl::Event::Key::arrow_right) || sl::key_down(sl::Event::Key::letter_d);
            input.up = sl::key_down(sl::Event::Key::arrow_up) || sl::key_down(sl::Event::Key::letter_w);
            input.down = sl::key_down(sl::Event::Key::arrow_down) || sl::key_down(sl::Event::Key::letter_s);
            input.bomb = sl::key_down(sl::Event::Key::space) || sl::key_down(sl::Event::Key::letter_z);
        }
        update(game, dt, input);

        draw_scene(renderer, game);
        sl::clear_to_colour(sl::screen, {0, 0, 0});
        sl::draw_sprite(sl::surface_3d_bitmap(renderer.surface), 0, 0);
        draw_hud(game);
        if (!shot_path.empty() && !saved && elapsed >= shot_at)
        {
            if (!sl::save_bitmap(sl::screen, shot_path))
            {
                std::fprintf(stderr, "exbomberman: could not save screenshot to %s\n", shot_path.c_str());
                shot_failed = true;
            }
            saved = true;
            running = false;
        }
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    if (game.audio.ready)
        sl::audio_fx_shutdown();
    renderer.bloom.shutdown();
    sl::destroy_mesh(renderer.cube);
    sl::destroy_surface_3d(renderer.surface);
    sl::shutdown();
    return shot_failed ? 1 : 0;
}
