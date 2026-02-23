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
#include "simlab/Scenario.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

namespace
{
    struct CoutRedirect
    {
        CoutRedirect()
        {
            m_old = std::cout.rdbuf(m_sink.rdbuf());
        }
        ~CoutRedirect()
        {
            std::cout.rdbuf(m_old);
        }

    private:
        std::streambuf*    m_old{nullptr};
        std::ostringstream m_sink;
    };

    float TotalKineticEnergy(ecs::World& world)
    {
        auto* rbStorage = world.GetStorage<physics::RigidBodyComponent>();
        if (!rbStorage)
        {
            return 0.0f;
        }

        float energy = 0.0f;
        rbStorage->ForEach([&](ecs::EntityId, physics::RigidBodyComponent& rb) {
            if (rb.mass <= 0.0f)
            {
                return;
            }
            const float speedSq = rb.vx * rb.vx + rb.vy * rb.vy;
            energy += 0.5f * rb.mass * speedSq;
        });
        return energy;
    }

    size_t CountDynamicBodies(ecs::World& world)
    {
        auto* rbStorage = world.GetStorage<physics::RigidBodyComponent>();
        if (!rbStorage)
        {
            return 0;
        }
        size_t count = 0;
        rbStorage->ForEach([&](ecs::EntityId, physics::RigidBodyComponent& rb) {
            if (rb.invMass > 0.0f)
            {
                ++count;
            }
        });
        return count;
    }

    bool HasDynamicMotion(ecs::World& world, float minSpeed)
    {
        auto* rbStorage = world.GetStorage<physics::RigidBodyComponent>();
        if (!rbStorage)
        {
            return false;
        }
        const float minSpeedSq = minSpeed * minSpeed;
        bool result = false;
        rbStorage->ForEach([&](ecs::EntityId, physics::RigidBodyComponent& rb) {
            if (rb.invMass == 0.0f)
            {
                return;
            }
            const float speedSq = rb.vx * rb.vx + rb.vy * rb.vy;
            if (speedSq > minSpeedSq)
            {
                result = true;
            }
        });
        return result;
    }

    void AssertFiniteState(ecs::World& world)
    {
        bool anyTransform = false;
        world.ForEach<physics::TransformComponent>([&](ecs::EntityId, physics::TransformComponent& tf) {
            assert(std::isfinite(tf.x));
            assert(std::isfinite(tf.y));
            assert(std::isfinite(tf.rotation));
            anyTransform = true;
        });
        assert(anyTransform && "Scenario should populate transforms");
    }

    void RunScenario(const char* name,
                     std::unique_ptr<simlab::IScenario> (*factory)())
    {
        std::cout << "\n=== " << name << " ===\n" << std::flush;

        ecs::World world;
        auto scenario = factory();
        scenario->Setup(world);

        const size_t dynamicBodies = CountDynamicBodies(world);
        assert(dynamicBodies > 0 && "Scenario must spawn dynamic bodies");

        const float initialEnergy = TotalKineticEnergy(world);

        bool motionDetected = false;
        const float dt = 1.0f / 60.0f;
        const int steps = 180; // 3 seconds of simulation at 60 fps

        for (int i = 0; i < steps; ++i)
        {
            scenario->Update(world, dt);
            world.Update(dt);
            scenario->Render(world, std::cout);
            AssertFiniteState(world);
            motionDetected = motionDetected || HasDynamicMotion(world, 0.05f);
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 fps
        }

        const float finalEnergy = TotalKineticEnergy(world);

        assert(motionDetected && "Scenario should exhibit measurable motion");
        assert(finalEnergy >= 0.0f);
        assert(finalEnergy >= initialEnergy || finalEnergy > 0.01f);
    }
}

int main()
{
    simlab::SetHeadlessRendering(false);
    RunScenario("gravity", simlab::CreatePlanetaryGravityScenario);
    RunScenario("wrecking", simlab::CreateWreckingBallScenario);
    RunScenario("fluid", simlab::CreateParticleFluidScenario);
    RunScenario("demo", simlab::CreateFullDemoScenario);

    std::cout << "Simlab scenario tests passed\n";
    return 0;
}
