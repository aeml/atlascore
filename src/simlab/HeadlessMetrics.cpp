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

#include "simlab/HeadlessMetrics.hpp"

#include "ecs/World.hpp"
#include "physics/Systems.hpp"
#include "simlab/WorldHasher.hpp"

#include <iomanip>
#include <ostream>

namespace simlab
{
    FrameMetrics CaptureFrameMetrics(const ecs::World& world,
                                     const physics::PhysicsSystem& physicsSystem,
                                     std::size_t frameIndex,
                                     double simTimeSeconds) noexcept
    {
        FrameMetrics metrics{};
        metrics.frameIndex = frameIndex;
        metrics.simTimeSeconds = simTimeSeconds;

        WorldHasher hasher;
        metrics.worldHash = hasher.HashWorld(world);
        metrics.collisionCount = physicsSystem.GetCollisionEvents().size();

        if (const auto* rigidBodies = world.GetStorage<physics::RigidBodyComponent>())
        {
            metrics.rigidBodyCount = rigidBodies->Size();
            rigidBodies->ForEach([&](ecs::EntityId, const physics::RigidBodyComponent& body) {
                if (body.invMass > 0.0f)
                {
                    ++metrics.dynamicBodyCount;
                }
            });
        }

        if (const auto* transforms = world.GetStorage<physics::TransformComponent>())
        {
            metrics.transformCount = transforms->Size();
        }

        return metrics;
    }

    void WriteFrameMetricsCsvHeader(std::ostream& out)
    {
        out << "frame,sim_time_seconds,world_hash,collision_count,rigid_body_count,dynamic_body_count,transform_count\n";
    }

    void WriteFrameMetricsCsvRow(std::ostream& out, const FrameMetrics& metrics)
    {
        const auto previousFlags = out.flags();
        const auto previousPrecision = out.precision();

        out << metrics.frameIndex << ','
            << std::fixed << std::setprecision(6) << metrics.simTimeSeconds << ','
            << metrics.worldHash << ','
            << metrics.collisionCount << ','
            << metrics.rigidBodyCount << ','
            << metrics.dynamicBodyCount << ','
            << metrics.transformCount << '\n';

        out.flags(previousFlags);
        out.precision(previousPrecision);
    }
}
