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

#include "core/Logger.hpp"
#include "ecs/World.hpp"
#include "simlab/Scenario.hpp"
#include "simlab/WorldHasher.hpp"
#include "physics/Components.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <memory>
#include <vector>

// Mock system for ECS testing
class MockSystem : public ecs::ISystem
{
public:
    void Update(ecs::World& world, float dt) override
    {
        (void)world;
        (void)dt;
        updated = true;
    }
    bool updated = false;
};

int main()
{
    std::cout << "Running AtlasCore coverage tests...\n";

    // ------------------------------------------------------------------------
    // Core: Logger Coverage
    // ------------------------------------------------------------------------
    {
        core::Logger logger;
        std::stringstream ss;
        auto sharedSS = std::shared_ptr<std::stringstream>(&ss, [](void*) {}); // No-op deleter
        logger.SetOutput(sharedSS);

        logger.Warn("Test Warning");
        logger.Error("Test Error");

        std::string output = ss.str();
        assert(output.find("WARN: Test Warning") != std::string::npos);
        assert(output.find("ERROR: Test Error") != std::string::npos);
        std::cout << "[PASS] Logger Warn/Error coverage\n";
    }

    // ------------------------------------------------------------------------
    // ECS: World Coverage
    // ------------------------------------------------------------------------
    {
        ecs::World world;
        auto e1 = world.CreateEntity();
        auto e2 = world.CreateEntity();
        (void)e2;

        // Test DestroyEntity (even if implementation is minimal, we must cover it)
        world.DestroyEntity(e1);
        
        // Test AddSystem with null check (though usually we pass valid systems)
        // The code has `if (system) m_systems.emplace_back(...)`
        world.AddSystem(nullptr); 

        auto sys = std::make_unique<MockSystem>();
        auto* sysPtr = sys.get();
        world.AddSystem(std::move(sys));
        
        world.Update(0.1f);
        assert(sysPtr->updated);
        (void)sysPtr; // Suppress unused variable warning
        std::cout << "[PASS] ECS World coverage\n";
    }

    // ------------------------------------------------------------------------
    // SimLab: Scenario Coverage (Execution)
    // ------------------------------------------------------------------------
    {
        ecs::World world;

        // 1. Smoke Test
        auto smoke = simlab::CreateDeterminismSmokeTest();
        if (smoke)
        {
            smoke->Setup(world);
            // Run a few steps to cover update logic, spawning, and rendering
            std::stringstream ss;
            for (int i = 0; i < 5; ++i)
            {
                smoke->Update(world, 1.0f / 60.0f);
                smoke->Render(world, ss);
            }
        }

        // 2. Hash Scenario
        auto hash = simlab::CreateDeterminismHashScenario();
        if (hash)
        {
            hash->Setup(world);
            hash->Update(world, 1.0f / 60.0f);
            std::stringstream ss;
            hash->Render(world, ss);
        }

        // 3. Text Patterns
        auto patterns = simlab::CreateTextRendererPatternsScenario();
        if (patterns)
        {
            patterns->Setup(world);
            patterns->Update(world, 0.1f);
            std::stringstream ss;
            patterns->Render(world, ss);
        }

        std::cout << "[PASS] SimLab Scenarios execution coverage\n";
    }

    // ------------------------------------------------------------------------
    // SimLab: WorldHasher Extra Coverage
    // ------------------------------------------------------------------------
    {
        simlab::WorldHasher hasher;
        std::vector<physics::AABBComponent> aabbs;
        aabbs.push_back({0.f, 0.f, 1.f, 1.f});
        aabbs.push_back({2.f, 2.f, 3.f, 3.f});

        std::uint64_t h = hasher.HashAABBs(aabbs);
        assert(h != 0); // Basic check that it produces something
        (void)h;

        std::cout << "[PASS] WorldHasher AABB coverage\n";
    }

    // ------------------------------------------------------------------------
    // SimLab: Scenario Registry Coverage
    // ------------------------------------------------------------------------
    {
        // Force instantiation of all registered scenarios to cover their factories
        const auto& all = simlab::ScenarioRegistry::All();
        for (const auto& desc : all)
        {
            if (desc.factory)
            {
                auto scenario = desc.factory();
                assert(scenario != nullptr);
            }
        }
        
        // Test FindFactory and Create
        auto factory = simlab::ScenarioRegistry::FindFactory("smoke");
        assert(factory != nullptr);
        (void)factory;
        
        auto instance = simlab::ScenarioRegistry::Create("smoke");
        assert(instance != nullptr);

        auto missing = simlab::ScenarioRegistry::Create("non_existent");
        assert(missing == nullptr);

        std::cout << "[PASS] SimLab ScenarioRegistry coverage\n";
    }

    std::cout << "All coverage tests passed.\n";
    return 0;
}
