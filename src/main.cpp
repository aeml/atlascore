#include "core/Logger.hpp"
#include "core/Clock.hpp"
#include "core/FixedTimestepLoop.hpp"
#include "ecs/World.hpp"
#include "simlab/Scenario.hpp"

#include <atomic>
#include <string>
#include <string_view>

int main(int argc, char** argv)
{
    core::Logger logger;
    logger.Info("AtlasCore starting up...");

    ecs::World world;
    // CLI scenario selection: default "smoke", optionally "hash"
    std::string scenarioName = "smoke";
    if (argc > 1 && argv[1])
    {
        scenarioName = argv[1];
    }

    std::unique_ptr<simlab::IScenario> scenario;
    if (scenarioName == "smoke")
    {
        scenario = simlab::CreateDeterminismSmokeTest();
    }
    else if (scenarioName == "hash")
    {
        scenario = simlab::CreateDeterminismHashScenario();
    }
    else
    {
        logger.Error(std::string("Unknown scenario: ") + scenarioName + ". Available: smoke, hash");
        scenario = simlab::CreateDeterminismSmokeTest();
    }
    scenario->Setup(world);

    std::atomic<bool> running{true};

    core::FixedTimestepLoop loop{1.0f / 60.0f};

    int steps = 0;

    loop.Run(
        [&](float dt)
        {
            scenario->Step(world, dt);
            world.Update(dt);

            if (++steps >= 60)
            {
                running.store(false);
            }
        },
        running);

    logger.Info("AtlasCore shutting down.");
    return 0;
}
