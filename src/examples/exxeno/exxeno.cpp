#include "sl.h"
#include "audio_synth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <utility>
#include <vector>

// A lit top-down shooter: a space marine fights xenomorphs in a tile-map arena. WASD moves, the mouse aims and the
// left button fires glowing bolts. Each kill stamps an acid splat on the floor with tilemap_add_decal(); the splats,
// walls, aliens and marine are then lit by a flashlight, a muzzle flash and every bolt in flight.
namespace
{
    constexpr int arena_columns = 25, arena_rows = 19, tile_size = 32;
    constexpr int screen_w = arena_columns * tile_size, screen_h = arena_rows * tile_size;
    constexpr float pi = 3.14159265f;

    std::mt19937 rng{12345};
    float random_between(float low, float high) { return std::uniform_real_distribution<float>(low, high)(rng); }

    struct Bolt
    {
        float x, y, vx, vy, life;
    };

    struct Alien
    {
        float x, y, angle, phase, speed, hit_flash;
        int hp;
    };

    /** A pool of emitters that each fire a short burst of particles at a point. */
    struct BurstPool
    {
        struct Burst
        {
            sl::ParticleEmitter emitter;
            float timer = 0.0f;
        };
        std::vector<Burst> bursts;
        std::size_t next = 0;

        BurstPool(const sl::EmitterConfig &config, int count)
        {
            bursts.resize(static_cast<std::size_t>(count));
            for (Burst &burst : bursts)
            {
                burst.emitter = sl::ParticleEmitter(config);
                burst.emitter.set_active(false);
            }
        }
        void fire(float x, float y)
        {
            Burst &burst = bursts[next++ % bursts.size()];
            burst.emitter.set_position(x, y);
            burst.emitter.set_active(true);
            burst.timer = 0.02f;
        }
        void update(float dt)
        {
            for (Burst &burst : bursts)
            {
                if (burst.timer > 0.0f && (burst.timer -= dt) <= 0.0f)
                    burst.emitter.set_active(false);
                burst.emitter.update(dt);
            }
        }
        void render() const
        {
            for (const Burst &burst : bursts)
                burst.emitter.render();
        }
        void clear()
        {
            for (Burst &burst : bursts)
            {
                burst.emitter.clear();
                burst.emitter.set_active(false);
            }
        }
    };

    /** Four 64x64 acid splat variants drawn into one render target, used as a one-row tileset. */
    sl::Bitmap *make_splat_sheet()
    {
        sl::Bitmap *sheet = sl::create_render_target(256, 64);
        if (!sheet)
            return nullptr;
        std::mt19937 local(7);
        const auto rnd = [&](float low, float high) { return std::uniform_real_distribution<float>(low, high)(local); };
        sl::begin_render_target(sheet);
        sl::clear_render_target({0, 0, 0, 0});
        for (int variant = 0; variant < 4; ++variant)
        {
            const float cx = 32.0f + variant * 64.0f, cy = 32.0f;
            for (int pass = 0; pass < 2; ++pass)
            {
                const sl::Colour colour = pass == 0 ? sl::Colour{40, 120, 20, 235} : sl::Colour{150, 235, 60, 235};
                const float shrink = pass == 0 ? 1.0f : 0.7f;
                sl::circlefill(sl::screen, cx, cy, (10.0f + variant * 1.5f) * shrink, colour);
                for (int blob = 0; blob < 11; ++blob)
                {
                    const float angle = rnd(0.0f, 2.0f * pi);
                    const float distance = rnd(6.0f, 22.0f);
                    const float radius = rnd(2.0f, 7.0f) * shrink;
                    sl::circlefill(sl::screen, cx + std::cos(angle) * distance, cy + std::sin(angle) * distance, radius, colour);
                    // A streak joins the larger blobs back to the centre.
                    if (radius > 4.0f)
                        sl::line(sl::screen, cx, cy, cx + std::cos(angle) * distance, cy + std::sin(angle) * distance,
                                 colour, radius * 0.7f);
                }
            }
        }
        sl::end_render_target();
        return sheet;
    }

    sl::TileMap *make_arena(sl::Bitmap *atlas_bitmap, sl::Bitmap *splat_sheet, std::vector<sl::ShadowCaster> &casters,
                            int &splat_first_gid)
    {
        const sl::Atlas floor_atlas = sl::create_atlas(atlas_bitmap, 32, 32);
        const sl::Atlas splat_atlas = sl::create_atlas(splat_sheet, 64, 64);
        sl::TileMap *map = floor_atlas.tile_count > 0 && splat_atlas.tile_count > 0
                               ? sl::create_tilemap(arena_columns, arena_rows, tile_size, tile_size)
                               : nullptr;
        if (!map)
            return nullptr;
        sl::tilemap_add_tileset(map, floor_atlas);
        sl::tilemap_add_tileset(map, splat_atlas);
        splat_first_gid = map->tilesets[1].first_gid;

        const auto tile = [](int x, int y) { return y * 16 + x + 1; };
        const int road = tile(11, 1), granite = tile(6, 9);
        const int floor_layer = sl::tilemap_add_layer(map, "floor");
        const int wall_layer = sl::tilemap_add_layer(map, "walls");
        std::vector<bool> solid(static_cast<std::size_t>(arena_columns * arena_rows), false);
        const auto block = [&](int column, int row, int columns, int rows)
        {
            for (int r = row; r < row + rows; ++r)
                for (int c = column; c < column + columns; ++c)
                    solid[static_cast<std::size_t>(r * arena_columns + c)] = true;
        };
        block(0, 0, arena_columns, 1);
        block(0, arena_rows - 1, arena_columns, 1);
        block(0, 0, 1, arena_rows);
        block(arena_columns - 1, 0, 1, arena_rows);
        block(6, 5, 2, 2);
        block(17, 5, 2, 2);
        block(6, 12, 2, 2);
        block(17, 12, 2, 2);
        block(11, 8, 3, 2);
        for (int row = 0; row < arena_rows; ++row)
        {
            for (int column = 0; column < arena_columns; ++column)
            {
                const std::uint32_t flip = (column * 7 + row * 3) % 4 == 0 ? sl::tile_flip_horizontal : 0;
                sl::tilemap_set(map, floor_layer, column, row, road | flip);
                if (solid[static_cast<std::size_t>(row * arena_columns + column)])
                    sl::tilemap_set(map, wall_layer, column, row, granite);
            }
        }
        sl::tilemap_set_solid(map, granite, true);

        const float t = static_cast<float>(tile_size);
        casters.push_back(sl::make_rectangle_shadow_caster(0, 0, screen_w, t));
        casters.push_back(sl::make_rectangle_shadow_caster(0, screen_h - t, screen_w, screen_h));
        casters.push_back(sl::make_rectangle_shadow_caster(0, 0, t, screen_h));
        casters.push_back(sl::make_rectangle_shadow_caster(screen_w - t, 0, screen_w, screen_h));
        casters.push_back(sl::make_rectangle_shadow_caster(6 * t, 5 * t, 8 * t, 7 * t));
        casters.push_back(sl::make_rectangle_shadow_caster(17 * t, 5 * t, 19 * t, 7 * t));
        casters.push_back(sl::make_rectangle_shadow_caster(6 * t, 12 * t, 8 * t, 14 * t));
        casters.push_back(sl::make_rectangle_shadow_caster(17 * t, 12 * t, 19 * t, 14 * t));
        casters.push_back(sl::make_rectangle_shadow_caster(11 * t, 8 * t, 14 * t, 10 * t));
        return map;
    }

    void draw_alien(const Alien &alien, float time)
    {
        const float dx = std::cos(alien.angle), dy = std::sin(alien.angle);
        const float sx = -dy, sy = dx;
        const sl::Colour shell = alien.hit_flash > 0.0f ? sl::Colour{200, 255, 160} : sl::Colour{22, 28, 26};
        const sl::Colour shine{75, 105, 90};
        const float skitter = std::sin(time * 14.0f + alien.phase);
        // Tail whips behind the body.
        float px = alien.x - dx * 8.0f, py = alien.y - dy * 8.0f;
        for (int segment = 1; segment <= 6; ++segment)
        {
            const float sway = std::sin(time * 6.0f + alien.phase + segment * 0.8f) * segment * 2.2f;
            const float nx = alien.x - dx * (8.0f + segment * 5.0f) + sx * sway;
            const float ny = alien.y - dy * (8.0f + segment * 5.0f) + sy * sway;
            sl::line(sl::screen, px, py, nx, ny, shell, 4.0f - segment * 0.45f);
            px = nx;
            py = ny;
        }
        // Four skittering legs.
        for (int leg = 0; leg < 4; ++leg)
        {
            const float side = leg % 2 == 0 ? 1.0f : -1.0f;
            const float along = (leg / 2 == 0 ? 5.0f : -3.0f) + skitter * side * 3.0f;
            const float bx = alien.x + dx * along, by = alien.y + dy * along;
            const float fx = bx + sx * side * 17.0f + dx * skitter * 4.0f, fy = by + sy * side * 17.0f + dy * skitter * 4.0f;
            sl::line(sl::screen, bx, by, fx, fy, shell, 2.0f);
            sl::line(sl::screen, fx, fy, fx + dx * 5.0f, fy + dy * 5.0f, shine, 1.5f);
        }
        // Thorax, abdomen and the long swept-back head.
        sl::circlefill(sl::screen, alien.x, alien.y, 9.0f, shell);
        sl::circlefill(sl::screen, alien.x - dx * 6.0f, alien.y - dy * 6.0f, 7.0f, shell);
        for (int step = 0; step < 4; ++step)
        {
            const float reach = 8.0f + step * 4.5f;
            sl::circlefill(sl::screen, alien.x + dx * reach, alien.y + dy * reach, 6.5f - step * 1.2f, shell);
        }
        sl::circlefill(sl::screen, alien.x + dx * 11.0f + sx * 2.5f, alien.y + dy * 11.0f + sy * 2.5f, 2.2f, shine);
        sl::circlefill(sl::screen, alien.x + dx * 26.0f, alien.y + dy * 26.0f, 1.8f, {170, 255, 110});
    }

    void draw_marine(float x, float y, float aim)
    {
        const float dx = std::cos(aim), dy = std::sin(aim);
        const float sx = -dy, sy = dx;
        sl::circlefill(sl::screen, x, y + 2.0f, 15.0f, {10, 10, 12, 140});
        sl::circlefill(sl::screen, x + sx * 10.0f, y + sy * 10.0f, 6.5f, {50, 90, 135});
        sl::circlefill(sl::screen, x - sx * 10.0f, y - sy * 10.0f, 6.5f, {50, 90, 135});
        sl::circlefill(sl::screen, x, y, 11.0f, {70, 120, 175});
        sl::line(sl::screen, x + dx * 4.0f - sx * 5.0f, y + dy * 4.0f - sy * 5.0f, x + dx * 28.0f - sx * 5.0f,
                 y + dy * 28.0f - sy * 5.0f, {40, 44, 52}, 5.0f);
        sl::line(sl::screen, x + dx * 22.0f - sx * 5.0f, y + dy * 22.0f - sy * 5.0f, x + dx * 29.0f - sx * 5.0f,
                 y + dy * 29.0f - sy * 5.0f, {120, 255, 210}, 3.0f);
        sl::circlefill(sl::screen, x + dx * 2.0f, y + dy * 2.0f, 7.0f, {205, 208, 215});
        sl::circlefill(sl::screen, x + dx * 5.0f, y + dy * 5.0f, 3.6f, {90, 230, 255});
    }


    /** Synthesised sound effects and the music loop; every sound is generated at start-up, so there are no audio assets. */
    struct Audio
    {
        sl::Sample *laser = nullptr, *impact = nullptr, *kill = nullptr, *hurt = nullptr, *spawn = nullptr,
                   *death = nullptr, *music = nullptr;
        std::uint64_t music_voice = 0;
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

    constexpr float music_bpm = 156.0f;
    constexpr int music_steps = 128;

    float midi_frequency(int note) { return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f); }

    /** Add one synthesised note into a mono float buffer, wrapping around the end so the loop is seamless. */
    void mix_note(std::vector<float> &buffer, sl::Waveform wave, float frequency, float start, float duration, float amplitude,
                  sl::Adsr envelope, float pitch_end = 1.0f)
    {
        sl::SynthParams params;
        params.duration = duration;
        params.envelope = envelope;
        params.oscillators.push_back({wave, frequency, 1.0f, {1.0f, pitch_end}});
        const std::vector<std::int16_t> note = sl::generate_audio(params);
        const std::size_t offset = static_cast<std::size_t>(start * static_cast<float>(params.sample_rate));
        for (std::size_t index = 0; index < note.size(); ++index)
            buffer[(offset + index) % buffer.size()] += static_cast<float>(note[index]) / 32768.0f * amplitude;
    }

    /** Eight bars of driving D-minor: four-on-the-floor kick, rolling saw bass, 16th arpeggio and snare/hat backbeat. */
    sl::Sample *make_music()
    {
        const float step_seconds = 60.0f / music_bpm / 4.0f;
        std::vector<float> buffer(static_cast<std::size_t>(step_seconds * music_steps * 44100.0f), 0.0f);
        const int roots[8] = {38, 38, 34, 36, 38, 38, 34, 33};
        const int chords[8][3] = {{62, 65, 69}, {62, 65, 69}, {58, 62, 65}, {60, 64, 67},
                                  {62, 65, 69}, {62, 65, 69}, {58, 62, 65}, {57, 61, 64}};
        for (int step = 0; step < music_steps; ++step)
        {
            const int bar = step / 16, beat_step = step % 16;
            const float t = static_cast<float>(step) * step_seconds;
            if (beat_step % 4 == 0)
                mix_note(buffer, sl::Waveform::sine, 160.0f, t, 0.22f, 0.9f, {0.001f, 0.08f, 0.0f, 0.1f}, 0.25f);
            if (beat_step == 4 || beat_step == 12)
            {
                mix_note(buffer, sl::Waveform::noise, 1.0f, t, 0.16f, 0.45f, {0.001f, 0.06f, 0.0f, 0.08f});
                mix_note(buffer, sl::Waveform::triangle, 220.0f, t, 0.1f, 0.3f, {0.001f, 0.05f, 0.0f, 0.05f}, 0.6f);
            }
            mix_note(buffer, sl::Waveform::noise, 1.0f, t, 0.045f, beat_step % 2 ? 0.22f : 0.12f, {0.001f, 0.02f, 0.0f, 0.02f});
            // Bass rolls on 8ths with an octave jump on the off-beats of beats 2 and 4.
            if (beat_step % 2 == 0)
            {
                const bool jump = beat_step == 6 || beat_step == 14;
                mix_note(buffer, sl::Waveform::saw, midi_frequency(roots[bar] + (jump ? 12 : 0)), t, step_seconds * 1.7f,
                         0.34f, {0.002f, 0.06f, 0.5f, 0.05f});
            }
            // Square arpeggio joins after two bars and climbs an octave for the last two.
            if (bar >= 2)
            {
                const int pattern[16] = {0, 1, 2, 3, 2, 1, 2, 3, 0, 1, 2, 3, 2, 3, 2, 1};
                const int index = pattern[beat_step];
                const int note = index == 3 ? chords[bar][0] + 12 : chords[bar][index];
                mix_note(buffer, sl::Waveform::square, midi_frequency(note + (bar >= 6 ? 24 : 12)), t, step_seconds * 0.9f,
                         0.12f, {0.002f, 0.04f, 0.4f, 0.03f});
            }
            // Chord stabs on the bar line, and a rising siren riser through the final bar.
            if (beat_step == 0 || beat_step == 10)
            {
                for (int tone = 0; tone < 3; ++tone)
                    mix_note(buffer, sl::Waveform::saw, midi_frequency(chords[bar][tone]), t, step_seconds * 2.5f, 0.1f,
                             {0.003f, 0.08f, 0.2f, 0.08f});
            }
            if (bar == 7 && beat_step % 2 == 0)
                mix_note(buffer, sl::Waveform::saw, 400.0f * std::pow(2.0f, beat_step / 8.0f), t, step_seconds * 1.6f, 0.1f,
                         {0.002f, 0.04f, 0.5f, 0.05f});
        }
        std::vector<std::int16_t> pcm(buffer.size());
        for (std::size_t index = 0; index < buffer.size(); ++index)
            pcm[index] = static_cast<std::int16_t>(std::tanh(buffer[index] * 0.8f) * 30000.0f);
        return sl::create_sample_from_pcm(pcm.data(), pcm.size(), 44100, 1);
    }

    bool init_audio(Audio &audio)
    {
        if (!sl::audio_fx_init())
            return false;
        using W = sl::Waveform;
        // Plasma bolt: a bright square zap falling in pitch over a detuned saw.
        audio.laser = synth(0.22f, {0.001f, 0.05f, 0.3f, 0.1f},
                            {{W::square, 1500.0f, 0.5f, {1.0f, 0.18f}}, {W::saw, 1530.0f, 0.3f, {1.0f, 0.2f}}});
        // Bolt striking a wall: a short crackle and a high ping.
        audio.impact = synth(0.12f, {0.001f, 0.03f, 0.2f, 0.06f},
                             {{W::noise, 1.0f, 0.5f, {}}, {W::triangle, 2400.0f, 0.4f, {1.0f, 0.5f}}});
        // Alien dying: a falling electronic shriek with a wet burst underneath.
        audio.kill = synth(0.55f, {0.002f, 0.2f, 0.3f, 0.25f},
                           {{W::saw, 900.0f, 0.4f, {1.0f, 0.12f}}, {W::square, 450.0f, 0.3f, {1.0f, 0.1f}},
                            {W::noise, 1.0f, 0.35f, {}}});
        // Marine hit: a low throb plus static.
        audio.hurt = synth(0.35f, {0.001f, 0.1f, 0.3f, 0.15f},
                           {{W::saw, 140.0f, 0.6f, {1.0f, 0.4f}}, {W::noise, 1.0f, 0.3f, {}}});
        // Alien emerging: a quiet rising warble.
        audio.spawn = synth(0.4f, {0.1f, 0.1f, 0.5f, 0.15f}, {{W::sine, 220.0f, 0.4f, {0.6f, 1.6f}}, {W::triangle, 330.0f, 0.2f, {0.6f, 1.7f}}});
        // Overrun: a long power-down sweep.
        audio.death = synth(1.6f, {0.01f, 0.3f, 0.6f, 0.6f},
                            {{W::saw, 600.0f, 0.5f, {1.0f, 0.05f}}, {W::square, 300.0f, 0.3f, {1.0f, 0.05f}}});
        audio.music = make_music();
        audio.ready = audio.laser && audio.impact && audio.kill && audio.hurt && audio.spawn && audio.death && audio.music;
        return audio.ready;
    }

    void shutdown_audio(Audio &audio)
    {
        sl::stop_all_samples();
        for (sl::Sample *sample : {audio.laser, audio.impact, audio.kill, audio.hurt, audio.spawn, audio.death, audio.music})
            sl::destroy_sample(sample);
        sl::audio_fx_shutdown();
    }

    void play(const Audio &audio, sl::Sample *sample, float x, int volume, int frequency = 1000)
    {
        if (audio.ready)
            sl::play_sample(sample, volume, std::clamp(static_cast<int>(x / screen_w * 255.0f), 0, 255), frequency);
    }

    bool blocked(const sl::TileMap *map, float x, float y, float radius)
    {
        return sl::tilemap_rect_blocked(map, x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    }
} // namespace

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
        return -1;

    Audio audio;
    init_audio(audio);
    sl::Bitmap *atlas_bitmap = sl::load_bitmap("assets/textures/atlas_grid.png");
    sl::Bitmap *splat_sheet = make_splat_sheet();
    sl::Bitmap *scene = sl::create_render_target(screen_w, screen_h);
    sl::LightingPass lighting;
    std::vector<sl::ShadowCaster> casters;
    int splat_first_gid = 0;
    sl::TileMap *map = atlas_bitmap && splat_sheet ? make_arena(atlas_bitmap, splat_sheet, casters, splat_first_gid) : nullptr;
    if (!map || !scene || !lighting.initialise())
    {
        std::fprintf(stderr, "exxeno: setup failed\n");
        shutdown_audio(audio);
        sl::destroy_tilemap(map);
        sl::destroy_bitmap(scene);
        sl::destroy_bitmap(splat_sheet);
        sl::destroy_bitmap(atlas_bitmap);
        sl::shutdown();
        return -1;
    }
    int audio_volume=28; 
    lighting.set_ambient(0.24f);
    if (audio.ready)
        sl::audio_fx_set_volume(audio_volume);
    if (audio.ready)
        audio.music_voice = sl::play_sample(audio.music, 100, audio_volume, 1000, -1);
    map->max_decals = 400;

    sl::EmitterConfig acid;
    acid.emit_rate = 2400.0f;
    acid.particle_lifetime = 0.55f;
    acid.min_speed = 40.0f;
    acid.max_speed = 260.0f;
    acid.spread_degrees = 180.0f;
    acid.start_size = 6.0f;
    acid.end_size = 1.0f;
    acid.start_colour = {140, 255, 60, 255};
    acid.end_colour = {20, 120, 20, 0};
    acid.max_particles = 120;
    sl::EmitterConfig sparks = acid;
    sparks.emit_rate = 1500.0f;
    sparks.particle_lifetime = 0.25f;
    sparks.min_speed = 80.0f;
    sparks.max_speed = 320.0f;
    sparks.start_size = 3.0f;
    sparks.start_colour = {255, 240, 170, 255};
    sparks.end_colour = {255, 120, 40, 0};
    sl::EmitterConfig muzzle = sparks;
    muzzle.emit_rate = 1200.0f;
    muzzle.spread_degrees = 25.0f;
    muzzle.min_speed = 120.0f;
    muzzle.max_speed = 380.0f;
    muzzle.start_colour = {160, 255, 230, 255};
    muzzle.end_colour = {40, 200, 255, 0};
    BurstPool acid_bursts(acid, 8), spark_bursts(sparks, 12);
    sl::ParticleEmitter muzzle_emitter(muzzle);
    muzzle_emitter.set_active(false);

    float player_x = screen_w * 0.5f, player_y = screen_h * 0.5f + 90.0f, health = 100.0f;
    std::vector<Bolt> bolts;
    std::vector<Alien> aliens;
    float fire_cooldown = 0.0f, muzzle_flash = 0.0f, spawn_timer = 1.0f, damage_cooldown = 0.0f, hurt_flash = 0.0f;
    float time = 0.0f, survived = 0.0f;
    int kills = 0;

    const auto restart = [&]()
    {
        player_x = screen_w * 0.5f;
        player_y = screen_h * 0.5f + 90.0f;
        health = 100.0f;
        bolts.clear();
        aliens.clear();
        sl::tilemap_clear_decals(map);
        acid_bursts.clear();
        spark_bursts.clear();
        muzzle_emitter.clear();
        spawn_timer = 1.0f;
        survived = 0.0f;
        kills = 0;
        if (audio.ready)
            audio.music_voice = sl::play_sample(audio.music, 100, 128, 1000, -1);
    };

    sl::set_fps(60);
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::letter_r && health <= 0.0f)
                restart();
            sl::display_handle_event(event);
        }
        const float dt = std::min(static_cast<float>(sl::get_frame_time()) / 1000.0f, 0.05f);
        time += dt;
        sl::update_tilemap(map, dt);
        acid_bursts.update(dt);
        spark_bursts.update(dt);
        const bool alive = health > 0.0f;

        const float aim_x = static_cast<float>(sl::mouse_x()) - player_x;
        const float aim_y = static_cast<float>(sl::mouse_y()) - player_y;
        const float aim = std::atan2(aim_y, aim_x);
        const float aim_dx = std::cos(aim), aim_dy = std::sin(aim);

        if (alive)
        {
            survived += dt;
            float move_x = 0.0f, move_y = 0.0f;
            if (sl::key_down(sl::Event::Key::letter_a))
                move_x -= 1.0f;
            if (sl::key_down(sl::Event::Key::letter_d))
                move_x += 1.0f;
            if (sl::key_down(sl::Event::Key::letter_w))
                move_y -= 1.0f;
            if (sl::key_down(sl::Event::Key::letter_s))
                move_y += 1.0f;
            const float length = std::sqrt(move_x * move_x + move_y * move_y);
            if (length > 0.0f)
            {
                const float step = 190.0f * dt / length;
                if (!blocked(map, player_x + move_x * step, player_y, 11.0f))
                    player_x += move_x * step;
                if (!blocked(map, player_x, player_y + move_y * step, 11.0f))
                    player_y += move_y * step;
            }

            fire_cooldown -= dt;
            if (sl::mouse_button_down(sl::MouseButton::left) && fire_cooldown <= 0.0f)
            {
                fire_cooldown = 0.11f;
                muzzle_flash = 0.06f;
                const float jitter = random_between(-0.035f, 0.035f);
                const float shot_dx = std::cos(aim + jitter), shot_dy = std::sin(aim + jitter);
                const float gun_x = player_x + aim_dx * 30.0f - aim_dy * 5.0f;
                const float gun_y = player_y + aim_dy * 30.0f + aim_dx * 5.0f;
                bolts.push_back({gun_x, gun_y, shot_dx * 760.0f, shot_dy * 760.0f, 1.2f});
                muzzle_emitter.set_position(gun_x, gun_y);
                muzzle_emitter.set_active(true);
                play(audio, audio.laser, gun_x, 200, 940 + static_cast<int>(random_between(0.0f, 120.0f)));
            }
            else
                muzzle_emitter.set_active(false);
        }
        else
            muzzle_emitter.set_active(false);
        muzzle_emitter.update(dt);
        muzzle_flash = std::max(muzzle_flash - dt, 0.0f);
        hurt_flash = std::max(hurt_flash - dt, 0.0f);

        // Waves get denser the longer the marine survives.
        spawn_timer -= dt;
        if (alive && spawn_timer <= 0.0f && aliens.size() < 40)
        {
            spawn_timer = std::max(1.4f - survived * 0.015f, 0.35f);
            for (int attempt = 0; attempt < 12; ++attempt)
            {
                const float x = random_between(1.5f, arena_columns - 1.5f) * tile_size;
                const float y = random_between(1.5f, arena_rows - 1.5f) * tile_size;
                if (std::hypot(x - player_x, y - player_y) < 280.0f || blocked(map, x, y, 14.0f))
                    continue;
                aliens.push_back({x, y, 0.0f, random_between(0.0f, 6.0f), random_between(70.0f, 120.0f) + survived * 0.8f, 0.0f,
                                  2 + static_cast<int>(random_between(0.0f, 2.0f))});
                play(audio, audio.spawn, x, 70, 900 + static_cast<int>(random_between(0.0f, 300.0f)));
                break;
            }
        }

        for (Alien &alien : aliens)
        {
            alien.hit_flash = std::max(alien.hit_flash - dt, 0.0f);
            const float to_x = player_x - alien.x, to_y = player_y - alien.y;
            const float distance = std::max(std::hypot(to_x, to_y), 0.001f);
            const float target = std::atan2(to_y, to_x);
            const float delta = std::remainder(target - alien.angle, 2.0f * pi);
            alien.angle += std::clamp(delta, -6.0f * dt, 6.0f * dt);
            const float step = alien.speed * dt * (0.75f + 0.25f * std::sin(time * 9.0f + alien.phase));
            const float mx = std::cos(alien.angle) * step, my = std::sin(alien.angle) * step;
            if (!blocked(map, alien.x + mx, alien.y, 12.0f))
                alien.x += mx;
            if (!blocked(map, alien.x, alien.y + my, 12.0f))
                alien.y += my;
            if (alive && distance < 24.0f && damage_cooldown <= 0.0f)
            {
                health = std::max(health - 12.0f, 0.0f);
                damage_cooldown = 0.45f;
                hurt_flash = 0.2f;
                if (health <= 0.0f)
                {
                    sl::stop_voice(audio.music_voice);
                    play(audio, audio.death, player_x, 255);
                }
                else
                    play(audio, audio.hurt, player_x, 255);
            }
        }
        for (std::size_t a = 0; a < aliens.size(); ++a)
        {
            for (std::size_t b = a + 1; b < aliens.size(); ++b)
            {
                const float ox = aliens[b].x - aliens[a].x, oy = aliens[b].y - aliens[a].y;
                const float gap = std::hypot(ox, oy);
                if (gap > 0.001f && gap < 22.0f)
                {
                    const float push = (22.0f - gap) * 0.5f;
                    aliens[a].x -= ox / gap * push;
                    aliens[a].y -= oy / gap * push;
                    aliens[b].x += ox / gap * push;
                    aliens[b].y += oy / gap * push;
                }
            }
        }
        damage_cooldown = std::max(damage_cooldown - dt, 0.0f);

        for (Bolt &bolt : bolts)
        {
            bolt.x += bolt.vx * dt;
            bolt.y += bolt.vy * dt;
            bolt.life -= dt;
            if (blocked(map, bolt.x, bolt.y, 2.0f))
            {
                spark_bursts.fire(bolt.x - bolt.vx * dt, bolt.y - bolt.vy * dt);
                play(audio, audio.impact, bolt.x, 130, 900 + static_cast<int>(random_between(0.0f, 300.0f)));
                bolt.life = 0.0f;
                continue;
            }
            for (Alien &alien : aliens)
            {
                if (alien.hp <= 0 || std::hypot(alien.x - bolt.x, alien.y - bolt.y) > 17.0f)
                    continue;
                alien.hp -= 1;
                alien.hit_flash = 0.08f;
                bolt.life = 0.0f;
                spark_bursts.fire(bolt.x, bolt.y);
                play(audio, audio.impact, bolt.x, 110, 700);
                if (alien.hp <= 0)
                {
                    ++kills;
                    play(audio, audio.kill, alien.x, 230, 850 + static_cast<int>(random_between(0.0f, 350.0f)));
                    acid_bursts.fire(alien.x, alien.y);
                    // The splat is a decal: a tile stamped at a free position, rotated, scaled and left on the floor.
                    const std::uint32_t variant = static_cast<std::uint32_t>(random_between(0.0f, 4.0f)) % 4;
                    sl::tilemap_add_decal(map, 0, static_cast<std::uint32_t>(splat_first_gid) + variant, alien.x,
                                          alien.y, random_between(0.0f, 360.0f), random_between(0.8f, 1.5f));
                }
                break;
            }
        }
        bolts.erase(std::remove_if(bolts.begin(), bolts.end(), [](const Bolt &bolt) { return bolt.life <= 0.0f; }), bolts.end());
        aliens.erase(std::remove_if(aliens.begin(), aliens.end(), [](const Alien &alien) { return alien.hp <= 0; }), aliens.end());

        // Scene: floor, splats and walls come from the tile map, then the characters.
        sl::begin_render_target(scene);
        sl::clear_render_target({0, 0, 0});
        sl::draw_tilemap(map, 0.0f, 0.0f, screen_w, screen_h);
        for (const Alien &alien : aliens)
            draw_alien(alien, time);
        if (alive)
            draw_marine(player_x, player_y, aim);
        sl::end_render_target();

        std::vector<sl::Light> lights;
        sl::Light flashlight;
        flashlight.x = player_x + aim_dx * 12.0f;
        flashlight.y = player_y + aim_dy * 12.0f;
        flashlight.radius = 430.0f;
        flashlight.intensity = alive ? 1.5f : 0.0f;
        flashlight.direction_x = aim_dx;
        flashlight.direction_y = aim_dy;
        flashlight.inner_angle = 10.0f;
        flashlight.outer_angle = 30.0f;
        flashlight.shadow_softness = 3.0f;
        flashlight.colour = {255, 244, 215};
        lights.push_back(flashlight);
        sl::Light glow;
        glow.x = player_x;
        glow.y = player_y;
        glow.radius = 110.0f;
        glow.intensity = alive ? 0.7f : 0.0f;
        glow.colour = {120, 190, 255};
        lights.push_back(glow);
        if (muzzle_flash > 0.0f)
        {
            sl::Light flash;
            flash.x = player_x + aim_dx * 30.0f;
            flash.y = player_y + aim_dy * 30.0f;
            flash.radius = 240.0f;
            flash.intensity = 2.0f;
            flash.shadow_softness = 2.0f;
            flash.colour = {140, 255, 220};
            lights.push_back(flash);
        }
        for (std::size_t index = 0; index < bolts.size() && lights.size() < 12; ++index)
        {
            sl::Light bolt_light;
            bolt_light.x = bolts[index].x;
            bolt_light.y = bolts[index].y;
            bolt_light.radius = 120.0f;
            bolt_light.intensity = 1.1f;
            bolt_light.colour = {90, 255, 200};
            lights.push_back(bolt_light);
        }
        lighting.apply(scene, lights, 0, 0, screen_w, screen_h, casters);

        // Glowing effects are drawn over the lit scene.
        sl::set_blend_mode(sl::BlendMode::additive);
        for (const Bolt &bolt : bolts)
        {
            const float speed = std::hypot(bolt.vx, bolt.vy);
            const float tx = bolt.x - bolt.vx / speed * 26.0f, ty = bolt.y - bolt.vy / speed * 26.0f;
            sl::line(sl::screen, tx, ty, bolt.x, bolt.y, {40, 200, 150}, 6.0f);
            sl::line(sl::screen, tx, ty, bolt.x, bolt.y, {210, 255, 245}, 2.5f);
            sl::circlefill(sl::screen, bolt.x, bolt.y, 6.0f, {60, 220, 170});
        }
        acid_bursts.render();
        spark_bursts.render();
        muzzle_emitter.render();
        sl::set_blend_mode(sl::BlendMode::normal);

        if (hurt_flash > 0.0f)
            sl::rectfill(sl::screen, 0.0f, 0.0f, screen_w, screen_h, {255, 0, 0, static_cast<Uint8>(hurt_flash * 450.0f)});
        const float mouse_px = static_cast<float>(sl::mouse_x()), mouse_py = static_cast<float>(sl::mouse_y());
        sl::circle(sl::screen, mouse_px, mouse_py, 9.0f, {120, 255, 220}, 1.5f);
        sl::line(sl::screen, mouse_px - 14.0f, mouse_py, mouse_px - 5.0f, mouse_py, {120, 255, 220}, 1.5f);
        sl::line(sl::screen, mouse_px + 5.0f, mouse_py, mouse_px + 14.0f, mouse_py, {120, 255, 220}, 1.5f);
        sl::rectfill(sl::screen, 44.0f, 44.0f, 204.0f, 56.0f, {20, 20, 24});
        sl::rectfill(sl::screen, 45.0f, 45.0f, 45.0f + 158.0f * health / 100.0f, 55.0f, {70, 220, 120});
        sl::gprintf(44, 62, {235, 235, 235}, "Kills %d   Splats %zu", kills, map->decals.size());
        sl::gprintf(44, 80, {170, 170, 170}, "WASD move  Mouse aim/fire  Esc quit");
        if (!alive)
        {
            sl::gprintf_center(screen_h / 2 - 20, {255, 90, 90}, "OVERRUN - %d kills", kills);
            sl::gprintf_center(screen_h / 2 + 8, {235, 235, 235}, "Press R to restart");
        }
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_tilemap(map);
    sl::destroy_bitmap(scene);
    sl::destroy_bitmap(splat_sheet);
    sl::destroy_bitmap(atlas_bitmap);
    lighting.shutdown();
    shutdown_audio(audio);
    sl::shutdown();
    return 0;
}
