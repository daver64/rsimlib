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

} // namespace sl