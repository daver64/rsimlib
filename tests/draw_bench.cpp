#include "sl.h"

#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>

// Draw-submission benchmark (not a ctest). Reports the time spent inside the draw_mesh loop (CPU recording) versus
// the whole frame, with parallel recording off and on. Usage: draw_bench --vulkan
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 640, 480))
        return 0;
    int counts[] = {2000, 10000, 50000};
    sl::Surface3D *surface = sl::create_surface_3d(640, 480, 4);
    sl::Mesh *cube = sl::create_cube_mesh();
    sl::Camera camera;
    camera.position = {0, 40, 80};
    camera.target = {0, 0, 0};
    for (const bool parallel : {false, true})
    for (int draws : counts)
    {
        sl::set_parallel_recording(parallel);
        double loop_ms = 0, frame_ms = 0;
        const int frames = 60;
        for (int f = 0; f < frames + 10; ++f)
        {
            const auto frame_start = std::chrono::steady_clock::now();
            sl::begin_surface_3d(surface, camera);
            sl::clear_surface_3d({10, 10, 20, 255});
            const auto loop_start = std::chrono::steady_clock::now();
            const int side = 1 + static_cast<int>(std::sqrt(static_cast<double>(draws)));
            for (int i = 0; i < draws; ++i)
            {
                sl::Material material;
                material.tint = {static_cast<std::uint8_t>(60 + i % 190), 120, 200, 255};
                sl::draw_mesh(cube, glm::translate(glm::mat4(1.0f), {(i % side - side / 2) * 1.5f, 0.0f, (i / side - side / 2) * 1.5f}),
                              material);
            }
            const auto loop_end = std::chrono::steady_clock::now();
            sl::end_surface_3d();
            sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
            sl::show_video_bitmap();
            sl::end_frame();
            const auto frame_end = std::chrono::steady_clock::now();
            if (f >= 10)
            {
                loop_ms += std::chrono::duration<double, std::milli>(loop_end - loop_start).count();
                frame_ms += std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
            }
        }
        std::printf("%-8s %6d draws: loop %.2f ms, frame %.2f ms, loop share %.0f%%, parallel draws/frame %u\n",
                    parallel ? "parallel" : "inline", draws, loop_ms / frames, frame_ms / frames,
                    100.0 * loop_ms / frame_ms, sl::get_frame_stats().parallel_draws);
    }
    sl::shutdown();
    return 0;
}
