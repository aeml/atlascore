#pragma once

#include <atomic>
#include <functional>

namespace core
{
    class FixedTimestepLoop
    {
    public:
        explicit FixedTimestepLoop(float timestepSeconds) noexcept;

        void Run(const std::function<void(float)>& update,
                 std::atomic<bool>& runningFlag) const;

    private:
        float m_timestepSeconds;
    };
}
