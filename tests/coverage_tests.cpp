#include "core/Logger.hpp"
#include "ecs/World.hpp"
#include "simlab/Scenario.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <memory>

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
    // SimLab: Scenario Coverage
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
                // We don't run them (too long/interactive), just create them.
            }
        }
        
        // Explicitly create specific ones if they aren't in the registry by default 
        // (though they should be via auto-registration)
        auto s1 = simlab::CreateDeterminismSmokeTest();
        assert(s1);
        auto s2 = simlab::CreateDeterminismHashScenario();
        assert(s2);
        auto s3 = simlab::CreateTextRendererPatternsScenario();
        assert(s3);

        std::cout << "[PASS] SimLab Scenario factories coverage\n";
    }

    std::cout << "All coverage tests passed.\n";
    return 0;
}
