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
    struct ScenarioOption
    {
        const char* key;
        const char* title;
        std::unique_ptr<simlab::IScenario> (*factory)();
    };

    std::vector<ScenarioOption> options = {
        {"smoke", "ECS falling bodies smoke test", &simlab::CreateDeterminismSmokeTest},
        {"hash",  "Determinism hash dual-run scenario", &simlab::CreateDeterminismHashScenario},
    };

    // Determine scenario by CLI arg or interactive menu
    std::unique_ptr<simlab::IScenario> scenario;
    if (argc > 1 && argv[1])
    {
        std::string scenarioName = argv[1];
        bool found = false;
        for (const auto& opt : options)
        {
            if (scenarioName == opt.key)
            {
                scenario = opt.factory();
                found = true;
                break;
            }
        }
        if (!found)
        {
            logger.Error(std::string("Unknown scenario: ") + scenarioName + ". Available: smoke, hash");
            scenario = options.front().factory();
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
