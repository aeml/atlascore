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

#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace ecs { class World; }
namespace physics { class PhysicsSystem; }

namespace simlab
{
    struct FrameMetrics
    {
        std::size_t frameIndex{0};
        double simTimeSeconds{0.0};
        std::uint64_t worldHash{0};
        std::size_t collisionCount{0};
        std::size_t rigidBodyCount{0};
        std::size_t dynamicBodyCount{0};
        std::size_t transformCount{0};
    };

    FrameMetrics CaptureFrameMetrics(const ecs::World& world,
                                     const physics::PhysicsSystem& physicsSystem,
                                     std::size_t frameIndex,
                                     double simTimeSeconds) noexcept;

    void WriteFrameMetricsCsvHeader(std::ostream& out);
    void WriteFrameMetricsCsvRow(std::ostream& out, const FrameMetrics& metrics);
}
