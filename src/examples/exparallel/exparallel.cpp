#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

// Many separate draw calls, recorded three ways: on one thread, spread over worker threads by the backend, and with
// worker threads that also build the draws (RenderQueue3D). On OpenGL all three look the same, because a GL context
// belongs to one thread; try --vulkan (or --d3d12 on Windows) to see the difference.
namespace
{
    constexpr int worker_count = 4;

    float wave(int x, int z, float time)
    {
        return std::sin(x * 0.2f + time * 1.5f) * std::cos(z * 0.2f + time) * 1.5f;
    }

    // Cube `index` of a `side` x `side` field: its matrix and tint depend only on the index and time.
    void describe_cube(int index, int side, float time, glm::mat4 &model, sl::Material &material)
    {
        const int x = index % side;
        const int z = index / side;
        model = sl::model_matrix({(x - side / 2) * 1.0f, wave(x, z, time), (z - side / 2) * 1.0f}, {0, 0, 0});
        material.tint = {static_cast<std::uint8_t>(110 + 140 * x / side), static_cast<std::uint8_t>(110 + 140 * z / side), 220,
                         255};
    }
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 1000, 640))
        return -1;

    sl::Surface3D *surface = sl::create_surface_3d(940, 520, 4);
    sl::Mesh *cube = sl::create_cube_mesh(0.7f);
    if (!surface || !cube)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend.\n");
        sl::shutdown();
        return -1;
    }

    const char *mode_names[3] = {"One thread records every draw", "Backend records draws on worker threads",
                                 "Workers build draws into RenderQueue3D; backend records in parallel"};
    int mode = 1;
    int side = 100;
    std::vector<sl::RenderQueue3D> queues(worker_count);
    sl::Camera camera;
    camera.target = {0.0f, 0.0f, 0.0f};

    bool running = true;
    float last_time = static_cast<float>(sl::time_ms()) * 0.001f;
    float time = 0.0f;
    float frame_ms = 16.0f;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space)
                mode = (mode + 1) % 3;
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::arrow_up)
                side = std::min(side + 20, 220);
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::arrow_down)
                side = std::max(side - 20, 20);
            sl::display_handle_event(event);
        }
        const float now = static_cast<float>(sl::time_ms()) * 0.001f;
        const float delta = now - last_time;
        last_time = now;
        time += delta;
        frame_ms += (delta * 1000.0f - frame_ms) * 0.05f;

        // Mode 0 turns parallel recording off; the other two leave it on.
        sl::set_parallel_recording(mode != 0);
        const int total = side * side;

        camera.position = {std::sin(time * 0.15f) * side * 0.6f, side * 0.3f, std::cos(time * 0.15f) * side * 0.6f};
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({20, 24, 40});
            sl::set_light_3d({{-0.4f, -1.0f, -0.3f}, 0.7f});
            if (mode < 2)
            {
                for (int i = 0; i < total; ++i)
                {
                    glm::mat4 model;
                    sl::Material material;
                    describe_cube(i, side, time, model, material);
                    sl::draw_mesh(cube, model, material);
                }
            }
            else
            {
                // Each thread fills its own queue; filling touches no rendering state.
                std::vector<std::thread> threads;
                for (int w = 0; w < worker_count; ++w)
                    threads.emplace_back(
                        [&, w]
                        {
                            sl::RenderQueue3D &queue = queues[w];
                            queue.clear();
                            for (int i = w * total / worker_count; i < (w + 1) * total / worker_count; ++i)
                            {
                                glm::mat4 model;
                                sl::Material material;
                                describe_cube(i, side, time, model, material);
                                queue.draw_mesh(cube, model, material);
                            }
                        });
                for (std::thread &thread : threads)
                    thread.join();
                // Submit on the main thread, in order.
                for (const sl::RenderQueue3D &queue : queues)
                    sl::submit_render_queue(queue);
            }
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {30, 30, 30});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 30, 60);

        const sl::FrameStats stats = sl::get_frame_stats();
        sl::gprintf_center(24, {232, 236, 244}, "%d cubes: %s", total, mode_names[mode]);
        sl::gprintf_center(562, {232, 236, 244}, "%.1f ms/frame   recording threads %u, parallel draws %u",
                           frame_ms, stats.recording_threads, stats.parallel_draws);
        sl::gprintf_center(582, {160, 170, 190}, "Space: switch mode   Up/Down: cube count   Escape: exit");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(cube);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}
