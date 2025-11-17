#include "core/FixedTimestepLoop.hpp"
#include "core/Clock.hpp"

namespace core
{
    FixedTimestepLoop::FixedTimestepLoop(float timestepSeconds) noexcept
        : m_timestepSeconds(timestepSeconds)
    {
    }

    void FixedTimestepLoop::Run(const std::function<void(float)>& update,
                                std::atomic<bool>&                 runningFlag) const
    {
        const double timestep = static_cast<double>(m_timestepSeconds);
        double        previous = Clock::NowSeconds();
        double        accumulator = 0.0;

        while (runningFlag.load())
        {
            const double current = Clock::NowSeconds();
            const double frameTime = current - previous;
            previous = current;
            accumulator += frameTime;

            while (accumulator >= timestep)
            {
                update(m_timestepSeconds);
                accumulator -= timestep;
            }
        }
    }
}
