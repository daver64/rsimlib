#pragma once

#include <cstdint>

namespace sl::detail
{
    /** Running counters for the frame in progress; end_frame() publishes and clears them. */
    struct FrameCounters
    {
        std::uint32_t mesh_draws = 0;
        std::uint32_t instanced_draws = 0;
        std::uint32_t instances = 0;
        std::uint32_t blended_draws = 0;
    };

    FrameCounters &frame_counters();
} // namespace sl::detail
