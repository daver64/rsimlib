#include "sl.h"

#include <chrono>
#include <iostream>
#include <vector>
#include <random>

struct Particle
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

static const char *compute_shader_src = R"(#version 430 core
layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;

struct Particle {
    float x, y;
    float vx, vy;
    float r, g, b;
    float life;
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

uniform float uDeltaTime;
uniform float uWidth;
uniform float uHeight;

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= particles.length()) return;

    Particle p = particles[gid];
    p.x += p.vx * uDeltaTime;
    p.y += p.vy * uDeltaTime;

    if (p.x < 0.0) { p.x = 0.0; p.vx = -p.vx; }
    if (p.x > uWidth) { p.x = uWidth; p.vx = -p.vx; }
    if (p.y < 0.0) { p.y = 0.0; p.vy = -p.vy; }
    if (p.y > uHeight) { p.y = uHeight; p.vy = -p.vy; }

    particles[gid] = p;
}
)";

int main(int argc, char **argv)
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        std::cerr << "Failed to initialize display.\n";
        return -1;
    }

    sl::set_window_title("simlib - Compute Shader Example (excompute)");

    constexpr std::size_t num_particles = 1000;
    std::vector<Particle> host_particles(num_particles);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> distX(50.0f, 750.0f);
    std::uniform_real_distribution<float> distY(50.0f, 550.0f);
    std::uniform_real_distribution<float> distVel(-150.0f, 150.0f);
    std::uniform_real_distribution<float> distColor(0.3f, 1.0f);

    for (auto &p : host_particles)
    {
        p.x = distX(rng);
        p.y = distY(rng);
        p.vx = distVel(rng);
        p.vy = distVel(rng);
        p.r = distColor(rng);
        p.g = distColor(rng);
        p.b = distColor(rng);
        p.life = 1.0f;
    }

    sl::StorageBuffer particle_buffer(num_particles * sizeof(Particle));
    if (!particle_buffer.upload(host_particles))
    {
        std::cerr << "Failed to upload particle storage buffer.\n";
        return -1;
    }

    sl::Shader compute_shader;
    if (!compute_shader.load_compute(compute_shader_src))
    {
        std::cerr << "Failed to load compute shader: " << compute_shader.error() << "\n";
        return -1;
    }

    sl::Font *font = sl::get_default_monospace_font();
    sl::Event event;
    bool running = true;
    auto last_time = std::chrono::high_resolution_clock::now();

    while (running)
    {
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit || event.type() == sl::Event::Type::key_down)
                running = false;
            sl::display_handle_event(event);
        }

        auto now = std::chrono::high_resolution_clock::now();
        float delta = std::chrono::duration<float>(now - last_time).count();
        last_time = now;
        if (delta > 0.1f) delta = 0.1f;

        particle_buffer.bind(0);
        compute_shader.set_uniform("uDeltaTime", delta);
        compute_shader.set_uniform("uWidth", 800.0f);
        compute_shader.set_uniform("uHeight", 600.0f);
        sl::dispatch_compute_for(compute_shader, static_cast<unsigned int>(num_particles), 1, 1, 16, 1, 1);
        sl::compute_barrier();

        particle_buffer.readback(host_particles);

        sl::clear_to_colour(sl::screen, sl::Colour{15, 15, 25});

        for (const auto &p : host_particles)
        {
            sl::Colour c{
                static_cast<std::uint8_t>(p.r * 255.0f),
                static_cast<std::uint8_t>(p.g * 255.0f),
                static_cast<std::uint8_t>(p.b * 255.0f),
                255
            };
            sl::circlefill(sl::screen, p.x, p.y, 3.0f, c);
        }

        if (font)
        {
            sl::textout(font, 10, 10, sl::Colour{255, 255, 255}, "Compute Shader GPU Particle Simulation");
            sl::textout(font, 10, 30, sl::Colour{180, 180, 200}, "1,000 particles updated via SSBO compute shader");
        }

        sl::show_video_bitmap();
        sl::end_frame();
    }

    return 0;
}