#pragma once

#include <cstdint>

namespace core
{
    class Clock
    {
    public:
        static double NowSeconds();
        static std::uint64_t NowMicroseconds();
    };
}
