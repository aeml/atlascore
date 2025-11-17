#include "core/Logger.hpp"
#include "core/Clock.hpp"
#include "core/FixedTimestepLoop.hpp"
#include "ecs/World.hpp"
#include "simlab/Scenario.hpp"

#include <atomic>

int main()
{
    core::Logger logger;
    logger.Info("AtlasCore starting up...");

    ecs::World world;
    auto       scenario = simlab::CreateDeterminismSmokeTest();
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
