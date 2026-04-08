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

#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "simlab/HeadlessMetrics.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    void VerifyFrameMetricsCapturePhysicsState()
    {
        ecs::World world;
        auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
        auto* physicsPtr = physicsSystem.get();
        physics::PhysicsSettings settings;
        settings.substeps = 1;
        settings.positionIterations = 1;
        settings.velocityIterations = 1;
        physicsSystem->SetSettings(settings);
        world.AddSystem(std::move(physicsSystem));

        auto a = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(a, physics::TransformComponent{0.0f, 0.0f, 0.0f});
        auto& rbA = world.AddComponent<physics::RigidBodyComponent>(a);
        rbA.mass = 1.0f;
        rbA.invMass = 1.0f;
        world.AddComponent<physics::AABBComponent>(a, physics::AABBComponent{-0.5f, -0.5f, 0.5f, 0.5f});

        auto b = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(b, physics::TransformComponent{0.25f, 0.0f, 0.0f});
        auto& rbB = world.AddComponent<physics::RigidBodyComponent>(b);
        rbB.mass = 1.0f;
        rbB.invMass = 1.0f;
        world.AddComponent<physics::AABBComponent>(b, physics::AABBComponent{-0.25f, -0.5f, 0.75f, 0.5f});

        world.Update(1.0f / 60.0f);

        const auto metrics = simlab::CaptureFrameMetrics(world, *physicsPtr, 3, 0.05);
        assert(metrics.frameIndex == 3u);
        assert(metrics.simTimeSeconds == 0.05);
        assert(metrics.worldHash != 0);
        assert(metrics.collisionCount >= 1u);
        assert(metrics.rigidBodyCount == 2u);
        assert(metrics.dynamicBodyCount == 2u);
        assert(metrics.transformCount == 2u);
    }

    void VerifyCsvWriterProducesStableHeaderAndRow()
    {
        simlab::FrameMetrics metrics{};
        metrics.frameIndex = 7;
        metrics.simTimeSeconds = 0.125;
        metrics.worldHash = 42;
        metrics.collisionCount = 3;
        metrics.rigidBodyCount = 5;
        metrics.dynamicBodyCount = 4;
        metrics.transformCount = 6;

        std::ostringstream out;
        simlab::WriteFrameMetricsCsvHeader(out);
        simlab::WriteFrameMetricsCsvRow(out, metrics);

        const std::string csv = out.str();
        assert(csv.find("frame,sim_time_seconds,world_hash,collision_count,rigid_body_count,dynamic_body_count,transform_count\n") == 0);
        assert(csv.find("7,0.125000,42,3,5,4,6\n") != std::string::npos);
    }
}

int main()
{
    VerifyFrameMetricsCapturePhysicsState();
    VerifyCsvWriterProducesStableHeaderAndRow();
    std::cout << "Headless metrics tests passed\n";
    return 0;
}
