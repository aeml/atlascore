#include "core/Clock.hpp"

#include <chrono>

namespace core
{
    double Clock::NowSeconds()
    {
        using clock = std::chrono::steady_clock;
        const auto now = clock::now().time_since_epoch();
        return std::chrono::duration<double>(now).count();
    }

    std::uint64_t Clock::NowMicroseconds()
    {
        using clock = std::chrono::steady_clock;
        const auto now = clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(now).count());
    }
}
