/*
 * Copyright (C) 2025 aeml
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
