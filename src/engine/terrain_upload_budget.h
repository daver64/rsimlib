#pragma once

#include <algorithm>
#include <cmath>

namespace sl::detail
{
    class TerrainUploadBudget
    {
    public:
        int next(int max_per_update, double max_per_second, double elapsed_seconds)
        {
            const int limit = std::max(max_per_update, 1);
            if (!std::isfinite(max_per_second) || max_per_second <= 0.0)
                return limit;

            // Keep catch-up after a stall bounded so one update cannot monopolize the render thread.
            const double credit_limit = std::max(1.0, std::min(static_cast<double>(limit), max_per_second * 0.1));
            credit_ = std::min(credit_limit, credit_ + max_per_second * std::clamp(elapsed_seconds, 0.0, 0.1));
            return std::min(limit, static_cast<int>(credit_));
        }

        void consume()
        {
            credit_ = std::max(0.0, credit_ - 1.0);
        }

    private:
        double credit_ = 0.0;
    };
}
