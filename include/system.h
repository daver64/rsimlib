#pragma once

#include <cstdint>

namespace sl
{

    /** Return elapsed milliseconds from SDL's monotonic timer. */
    std::uint64_t time_ms();
    /** Delay execution for approximately the requested number of milliseconds. */
    void rest(std::uint32_t milliseconds);
    /** Shut down the system and release any allocated resources. */
    void shutdown();
    /** Set the target frame rate for end_frame() to maintain; 0 runs uncapped. */
    void set_fps(int fps);
    /** Return the currently configured target frame rate (0 = uncapped). */
    int get_fps();
    /** Call once per loop iteration; sleeps as needed to hit the target frame rate and records the frame time. */
    void end_frame();
    /** Return the duration of the last completed frame in milliseconds. */
    double get_frame_time();

    /** Per-frame diagnostics captured by end_frame(). Counts cover the frame that just finished. */
    struct FrameStats
    {
        double frame_time_ms = 0.0;    ///< Duration of the last frame.
        double average_fps = 0.0;      ///< Frames per second averaged over roughly half a second.
        std::uint32_t mesh_draws = 0;  ///< draw_mesh() calls issued on 3D surfaces.
        std::uint32_t instanced_draws = 0; ///< draw_mesh_instanced() calls issued.
        std::uint32_t instances = 0;   ///< Total instances submitted by instanced draws.
        std::uint32_t blended_draws = 0;   ///< Mesh draws (plain or instanced) using alpha or additive blending.
        std::uint64_t frame_number = 0;    ///< Number of frames completed since start-up.
    };

    /** Return the diagnostics for the last completed frame. */
    FrameStats get_frame_stats();

} // namespace sl