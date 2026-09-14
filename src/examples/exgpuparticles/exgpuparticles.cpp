#include "sl.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
    constexpr std::size_t NUM_PARTICLES = 150000;
    constexpr int SCREEN_WIDTH = 1280;
    constexpr int SCREEN_HEIGHT = 800;
    constexpr int MAX_FORCES = 16;
    constexpr float WELL_STRENGTH = -600000.0f;
    constexpr float EXPLOSION_STRENGTH = 900000.0f;
    constexpr float EXPLOSION_LIFETIME = 0.7f;

    // std430-friendly layout, matches the `Particle` struct in compute_shader_src.
    struct GpuParticle
    {
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float life = 1.0f;
    };

    // A gravity well (negative strength) or explosion impulse (positive strength).
    struct ForceSource
    {
        float x = 0.0f;
        float y = 0.0f;
        float strength = 0.0f;
        float radius = 0.0f;
    };

    struct Explosion
    {
        ForceSource force;
        float remaining = 0.0f;
    };

    const char *compute_shader_src = R"(#version 430 core
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct Particle {
    float x, y;
    float vx, vy;
    float r, g, b;
    float life;
};

struct Force {
    float x, y;
    float strength;
    float radius;
};

layout(std430, binding = 0) buffer ParticleBuffer { Particle particles[]; };
layout(std430, binding = 1) readonly buffer ForceBuffer { Force forces[]; };

uniform float uDeltaTime;
uniform float uWidth;
uniform float uHeight;
uniform int uForceCount;
uniform float uSeed;

float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= particles.length()) return;

    Particle p = particles[gid];
    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);
    vec2 accel = vec2(0.0);

    for (int i = 0; i < uForceCount; ++i) {
        Force f = forces[i];
        if (abs(f.strength) < 0.0001) continue;
        vec2 dir = pos - vec2(f.x, f.y);
        float dist2 = dot(dir, dir) + 400.0;
        accel += normalize(dir) * (f.strength / dist2);
    }

    vel += accel * uDeltaTime;
    vel *= 0.994;
    pos += vel * uDeltaTime;

    if (pos.x < 0.0) pos.x += uWidth;
    if (pos.x > uWidth) pos.x -= uWidth;
    if (pos.y < 0.0) pos.y += uHeight;
    if (pos.y > uHeight) pos.y -= uHeight;

    p.life -= uDeltaTime * 0.12;

    if (p.life <= 0.0) {
        float seed = float(gid) + uSeed;
        pos = vec2(hash11(seed) * uWidth, hash11(seed + 17.23) * uHeight);
        float angle = hash11(seed + 3.71) * 6.28318530718;
        float speed = 20.0 + hash11(seed + 9.13) * 60.0;
        vel = vec2(cos(angle), sin(angle)) * speed;
        p.r = 0.4 + hash11(seed + 1.0) * 0.6;
        p.g = 0.4 + hash11(seed + 2.0) * 0.6;
        p.b = 0.4 + hash11(seed + 3.0) * 0.6;
        p.life = 1.0;
    }

    p.x = pos.x; p.y = pos.y;
    p.vx = vel.x; p.vy = vel.y;
    particles[gid] = p;
}
)";

    // Vulkan's GLSL profile rejects loose `uniform` globals in compute shaders ("non-opaque
    // uniforms outside a block"), so the same values are exposed here as a push-constant block
    // instead; ShaderUniform entries below map each field name to its byte offset.
    const char *compute_shader_src_vulkan = R"(#version 430 core
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct Particle {
    float x, y;
    float vx, vy;
    float r, g, b;
    float life;
};

struct Force {
    float x, y;
    float strength;
    float radius;
};

layout(std430, binding = 0) buffer ParticleBuffer { Particle particles[]; };
layout(std430, binding = 1) readonly buffer ForceBuffer { Force forces[]; };

layout(push_constant) uniform Params {
    float uDeltaTime;
    float uWidth;
    float uHeight;
    int uForceCount;
    float uSeed;
} params;

float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= particles.length()) return;

    Particle p = particles[gid];
    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);
    vec2 accel = vec2(0.0);

    for (int i = 0; i < params.uForceCount; ++i) {
        Force f = forces[i];
        if (abs(f.strength) < 0.0001) continue;
        vec2 dir = pos - vec2(f.x, f.y);
        float dist2 = dot(dir, dir) + 400.0;
        accel += normalize(dir) * (f.strength / dist2);
    }

    vel += accel * params.uDeltaTime;
    vel *= 0.994;
    pos += vel * params.uDeltaTime;

    if (pos.x < 0.0) pos.x += params.uWidth;
    if (pos.x > params.uWidth) pos.x -= params.uWidth;
    if (pos.y < 0.0) pos.y += params.uHeight;
    if (pos.y > params.uHeight) pos.y -= params.uHeight;

    p.life -= params.uDeltaTime * 0.12;

    if (p.life <= 0.0) {
        float seed = float(gid) + params.uSeed;
        pos = vec2(hash11(seed) * params.uWidth, hash11(seed + 17.23) * params.uHeight);
        float angle = hash11(seed + 3.71) * 6.28318530718;
        float speed = 20.0 + hash11(seed + 9.13) * 60.0;
        vel = vec2(cos(angle), sin(angle)) * speed;
        p.r = 0.4 + hash11(seed + 1.0) * 0.6;
        p.g = 0.4 + hash11(seed + 2.0) * 0.6;
        p.b = 0.4 + hash11(seed + 3.0) * 0.6;
        p.life = 1.0;
    }

    p.x = pos.x; p.y = pos.y;
    p.vx = vel.x; p.vy = vel.y;
    particles[gid] = p;
}
)";

    const std::vector<sl::ShaderUniform> compute_vulkan_uniforms = {
        {"uDeltaTime", 0, 4},
        {"uWidth", 4, 4},
        {"uHeight", 8, 4},
        {"uForceCount", 12, 4},
        {"uSeed", 16, 4},
    };
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, SCREEN_WIDTH, SCREEN_HEIGHT))
    {
        std::fprintf(stderr, "Failed to initialize display: %s\n", sl::last_error().c_str());
        return -1;
    }
    std::fprintf(stderr,"Graphics backend: %s\n", sl::graphics_backend_name().c_str());
    sl::set_window_title("simlib - GPU Compute Particles (exgpuparticles)");

    std::vector<GpuParticle> host_particles(NUM_PARTICLES);
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> distX(0.0f, static_cast<float>(SCREEN_WIDTH));
    std::uniform_real_distribution<float> distY(0.0f, static_cast<float>(SCREEN_HEIGHT));
    std::uniform_real_distribution<float> distVel(-40.0f, 40.0f);
    std::uniform_real_distribution<float> distColour(0.4f, 1.0f);
    std::uniform_real_distribution<float> distLife(0.1f, 1.0f);

    for (auto &p : host_particles)
    {
        p.x = distX(rng);
        p.y = distY(rng);
        p.vx = distVel(rng);
        p.vy = distVel(rng);
        p.r = distColour(rng);
        p.g = distColour(rng);
        p.b = distColour(rng);
        p.life = distLife(rng);
    }

    sl::StorageBuffer particle_buffer(NUM_PARTICLES * sizeof(GpuParticle));
    if (!particle_buffer.upload(host_particles))
    {
        std::fprintf(stderr, "Failed to upload particle storage buffer.\n");
        return -1;
    }

    std::vector<ForceSource> host_forces(MAX_FORCES);
    sl::StorageBuffer force_buffer(MAX_FORCES * sizeof(ForceSource));
    force_buffer.upload(host_forces);

    sl::Shader compute_shader;
    const bool is_vulkan = sl::graphics_backend() == sl::GraphicsBackend::vulkan;
    const bool compute_loaded = is_vulkan
        ? compute_shader.load_compute(compute_shader_src_vulkan, compute_vulkan_uniforms)
        : compute_shader.load_compute(compute_shader_src);
    if (!compute_loaded)
    {
        std::fprintf(stderr, "Failed to load compute shader: %s\n", compute_shader.error().c_str());
        return -1;
    }

    std::vector<sl::PointVertex> points(NUM_PARTICLES);
    std::vector<Explosion> explosions;

    sl::Bitmap *scene = sl::create_render_target(SCREEN_WIDTH, SCREEN_HEIGHT);
    sl::Blur glow;
    const bool glow_ready = scene && glow.initialise();
    glow.set_radius(3.0f);
    glow.set_iterations(2);
    glow.set_opacity(1.3f);
    bool use_glow = true;

    bool running = true;
    bool well_active = false;
    int force_count = 0;
    float seed_accum = 0.0f;
    auto last_time = std::chrono::high_resolution_clock::now();

    sl::set_fps(60);

    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            else if (event.type() == sl::Event::Type::key_down && !event.key_repeat() && event.key() == sl::Event::Key::letter_b)
            {
                use_glow = !use_glow;
            }
            else if (event.type() == sl::Event::Type::mouse_button_down)
            {
                if (event.mouse_button() == 1)
                {
                    well_active = true;
                }
                else if (event.mouse_button() == 3 && explosions.size() < static_cast<std::size_t>(MAX_FORCES - 1))
                {
                    Explosion explosion;
                    explosion.force.x = static_cast<float>(sl::mouse_x());
                    explosion.force.y = static_cast<float>(sl::mouse_y());
                    explosion.force.strength = EXPLOSION_STRENGTH;
                    explosion.remaining = EXPLOSION_LIFETIME;
                    explosions.push_back(explosion);
                }
            }
            else if (event.type() == sl::Event::Type::mouse_button_up && event.mouse_button() == 1)
            {
                well_active = false;
            }
            sl::display_handle_event(event);
        }

        auto now = std::chrono::high_resolution_clock::now();
        float delta = std::chrono::duration<float>(now - last_time).count();
        last_time = now;
        if (delta > 0.1f) delta = 0.1f;
        seed_accum += delta;

        // Explosions fade out over their lifetime, then get dropped from the list.
        for (std::size_t i = 0; i < explosions.size();)
        {
            explosions[i].remaining -= delta;
            explosions[i].force.strength = EXPLOSION_STRENGTH * std::max(0.0f, explosions[i].remaining / EXPLOSION_LIFETIME);
            if (explosions[i].remaining <= 0.0f)
                explosions.erase(explosions.begin() + static_cast<long>(i));
            else
                ++i;
        }

        std::fill(host_forces.begin(), host_forces.end(), ForceSource{});
        force_count = 0;
        if (well_active)
        {
            host_forces[0].x = static_cast<float>(sl::mouse_x());
            host_forces[0].y = static_cast<float>(sl::mouse_y());
            host_forces[0].strength = WELL_STRENGTH;
            force_count = 1;
        }
        for (std::size_t i = 0; i < explosions.size() && force_count < MAX_FORCES; ++i)
        {
            host_forces[static_cast<std::size_t>(force_count)] = explosions[i].force;
            ++force_count;
        }
        force_buffer.upload(host_forces);

        // Vulkan requires a frame to already be active before dispatching compute work, so
        // start this frame's target here rather than after the dispatch.
        if (scene)
        {
            sl::begin_render_target(scene);
            sl::clear_render_target(sl::Colour{8, 10, 18});
        }
        else
        {
            sl::clear_to_colour(sl::screen, sl::Colour{8, 10, 18});
        }

        // Vulkan's storage-buffer bindings are recorded against whichever program is "in use",
        // so the compute program must be bound before binding buffers or dispatching.
        compute_shader.use();
        particle_buffer.bind(0);
        force_buffer.bind(1);
        compute_shader.set_uniform("uDeltaTime", delta);
        compute_shader.set_uniform("uWidth", static_cast<float>(SCREEN_WIDTH));
        compute_shader.set_uniform("uHeight", static_cast<float>(SCREEN_HEIGHT));
        compute_shader.set_uniform("uForceCount", force_count);
        compute_shader.set_uniform("uSeed", seed_accum);
        sl::dispatch_compute_for(compute_shader, static_cast<unsigned int>(NUM_PARTICLES), 1, 1, 256, 1, 1);
        sl::compute_barrier();

        particle_buffer.readback(host_particles);

        for (std::size_t i = 0; i < NUM_PARTICLES; ++i)
        {
            const GpuParticle &p = host_particles[i];
            const std::uint8_t alpha = static_cast<std::uint8_t>(std::clamp(p.life, 0.0f, 1.0f) * 255.0f);
            points[i].x = p.x;
            points[i].y = p.y;
            points[i].colour = sl::Colour{
                static_cast<std::uint8_t>(p.r * 255.0f),
                static_cast<std::uint8_t>(p.g * 255.0f),
                static_cast<std::uint8_t>(p.b * 255.0f),
                alpha};
        }

        sl::set_blend_mode(sl::BlendMode::additive);
        sl::draw_points(sl::screen, points.data(), static_cast<int>(points.size()));
        sl::set_blend_mode(sl::BlendMode::normal);

        if (scene)
        {
            sl::end_render_target();
            sl::clear_to_colour(sl::screen, sl::Colour{8, 10, 18});
            // Sharp particles first, then an additive blurred copy bleeds a soft halo around them.
            sl::draw_sprite(scene, 0.0f, 0.0f);
            if (use_glow && glow_ready)
            {
                sl::set_blend_mode(sl::BlendMode::additive);
                glow.apply(scene, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                sl::set_blend_mode(sl::BlendMode::normal);
            }
        }

        sl::rectfill(sl::screen, 4.0f, 4.0f, 470.0f, 68.0f, sl::Colour{0, 0, 0, 150});
        sl::gprintf(10, 10, {255, 255, 255}, "GPU Compute Particles (exgpuparticles)");
        sl::gprintf(10, 30, {180, 180, 200}, "%zu particles | %d active force sources | Glow: %s (B)", NUM_PARTICLES, force_count, use_glow ? "ON" : "OFF");
        sl::gprintf(10, 50, {180, 180, 200}, "Left Click/Drag: Gravity Well | Right Click: Explosion | Escape: Exit");

        sl::show_video_bitmap();
        sl::end_frame();
    }

    glow.shutdown();
    if (scene)
        sl::destroy_bitmap(scene);
    sl::shutdown();

    return 0;
}
