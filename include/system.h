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
        std::uint64_t pipelines_created = 0; ///< Pipeline states built since start-up (Vulkan and Direct3D 12; 0 on OpenGL). A rise mid-game is a compile stall.
        std::uint32_t parallel_draws = 0;    ///< Mesh draws recorded on worker threads during the last frame (see parallel_render.h).
        std::uint32_t recording_threads = 1; ///< Threads available for command recording; 1 on OpenGL or when parallel recording is off.
    };

    /** Return the diagnostics for the last completed frame. */
    FrameStats get_frame_stats();

    /** A nonblocking GPU diagnostic sample. Memory fields are independently available.
     * Usage and budget are process-scoped when memory_process_scope is true, otherwise adapter-scoped.
     * Free and total refer to the adapter's reported pool, not the process. */
    struct GpuDiagnostics
    {
        bool timing_supported = false;
        bool timing_valid = false;
        double elapsed_ms = 0.0;
        std::uint64_t frame_number = 0;
        bool memory_valid = false;
        bool memory_process_scope = false;
        std::uint64_t memory_usage_bytes = 0;
        std::uint64_t memory_budget_bytes = 0;
        bool memory_budget_valid = false;
        bool memory_free_valid = false;
        std::uint64_t memory_free_bytes = 0;
        bool memory_total_valid = false;
        std::uint64_t memory_total_bytes = 0;
    };

    /** Enable or disable GPU diagnostics. Disabled or newly enabled diagnostics discard old samples. */
    void set_gpu_diagnostics_enabled(bool enabled);
    /** Mark the beginning of a frame with a monotonically increasing application frame number.
     * The renderer closes it at presentation; frames may be skipped if no query slot is free. */
    void gpu_diagnostics_begin_frame(std::uint64_t frame_number);
    /** Retrieve the newest completed sample without waiting for GPU completion.
     * GPU elapsed time is between GPU markers, including idle time between them; it is not utilization.
     * Memory queries may be relatively expensive, so request them at a bounded interval. */
    GpuDiagnostics get_gpu_diagnostics(bool include_memory = true);

} // namespace sl