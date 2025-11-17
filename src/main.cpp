#include "core/Logger.hpp"
#include "core/Clock.hpp"
#include "core/FixedTimestepLoop.hpp"
#include "ecs/World.hpp"
#include "simlab/Scenario.hpp"

#include <atomic>
#include <string>
#include <string_view>
#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
    core::Logger logger;
    logger.Info("AtlasCore starting up...");

    ecs::World world;
    const auto& options = simlab::ScenarioRegistry::All();

    // Determine scenario by CLI arg or interactive menu
    std::unique_ptr<simlab::IScenario> scenario;
    if (argc > 1 && argv[1])
    {
        std::string scenarioName = argv[1];
        auto factory = simlab::ScenarioRegistry::FindFactory(scenarioName);
        if (factory)
        {
            scenario = factory();
        }
        else
        {
            logger.Error(std::string("Unknown scenario: ") + scenarioName);
            if (!options.empty())
            {
                scenario = options.front().factory();
            }
        }
    }
    else
    {
        std::cout << "Select a simulation to run:" << '\n';
        for (size_t i = 0; i < options.size(); ++i)
        {
            std::cout << "  [" << (i + 1) << "] " << options[i].title << " (" << options[i].key << ")" << '\n';
        }
        std::cout << "Enter choice number (default 1): ";
        std::string line;
        std::getline(std::cin, line);
        size_t choice = 1;
        if (!line.empty())
        {
            try
            {
                size_t pos = 0;
                unsigned long parsed = std::stoul(line, &pos);
                if (pos == line.size() && parsed >= 1 && parsed <= options.size())
                {
                    choice = static_cast<size_t>(parsed);
                }
            }
            catch (...)
            {
                // fallback to default
            }
        }
        scenario = options[choice - 1].factory();
        logger.Info(std::string("Running scenario: ") + options[choice - 1].key);
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
