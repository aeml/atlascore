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
#include <thread>

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
        // Build category -> subcategory -> indices mapping
        struct IndexRef { size_t idx; };
        std::vector<const char*> categories;
        for (const auto& d : options)
        {
            if (d.category)
            {
                bool seen = false;
                for (auto c : categories) if (std::string(c) == d.category) { seen = true; break; }
                if (!seen) categories.push_back(d.category);
            }
        }

        auto promptChoice = [](size_t max, const char* msg) -> size_t {
            std::cout << msg;
            std::string line; std::getline(std::cin, line);
            size_t choice = 1;
            if (!line.empty()) {
                try { size_t pos=0; unsigned long p = std::stoul(line, &pos); if (pos==line.size() && p>=1 && p<=max) choice = static_cast<size_t>(p); } catch(...) {}
            }
            return choice;
        };

        if (!categories.empty())
        {
            std::cout << "Select a simulation to run:" << '\n';
            size_t lineNo = 1;
            for (auto c : categories)
            {
                std::cout << "  [" << lineNo++ << "] " << c << '\n';
            }
            std::cout << "  [" << lineNo++ << "] All scenarios" << '\n';
            size_t catChoice = promptChoice(lineNo-1, "Enter choice number (default 1): ");

            if (catChoice <= categories.size())
            {
                const char* selectedCat = categories[catChoice-1];
                // Collect subcategories for this category
                std::vector<const char*> subcats;
                for (const auto& d : options)
                {
                    if (d.category && std::string(d.category) == selectedCat && d.subcategory)
                    {
                        bool seen=false; for (auto s: subcats) if (std::string(s)==d.subcategory) { seen=true; break; }
                        if (!seen) subcats.push_back(d.subcategory);
                    }
                }
                if (subcats.empty())
                {
                    // Directly list scenarios
                    std::vector<size_t> idxs;
                    for (size_t i=0;i<options.size();++i) if (options[i].category && std::string(options[i].category)==selectedCat) idxs.push_back(i);
                    std::cout << "Select scenario:" << '\n';
                    for (size_t i=0;i<idxs.size();++i) std::cout << "  [" << (i+1) << "] " << options[idxs[i]].title << " (" << options[idxs[i]].key << ")" << '\n';
                    size_t sc = promptChoice(idxs.size(), "Enter choice number (default 1): ");
                    scenario = options[idxs[sc-1]].factory();
                    logger.Info(std::string("Running scenario: ") + options[idxs[sc-1]].key);
                }
                else
                {
                    std::cout << "Select a subgroup:" << '\n';
                    for (size_t i=0;i<subcats.size();++i) std::cout << "  [" << (i+1) << "] " << subcats[i] << '\n';
                    size_t sg = promptChoice(subcats.size(), "Enter choice number (default 1): ");
                    const char* selectedSub = subcats[sg-1];
                    std::vector<size_t> idxs;
                    for (size_t i=0;i<options.size();++i)
                    {
                        const auto& d = options[i];
                        if (d.category && d.subcategory && std::string(d.category)==selectedCat && std::string(d.subcategory)==selectedSub)
                            idxs.push_back(i);
                    }
                    std::cout << "Select scenario:" << '\n';
                    for (size_t i=0;i<idxs.size();++i) std::cout << "  [" << (i+1) << "] " << options[idxs[i]].title << " (" << options[idxs[i]].key << ")" << '\n';
                    size_t sc = promptChoice(idxs.size(), "Enter choice number (default 1): ");
                    scenario = options[idxs[sc-1]].factory();
                    logger.Info(std::string("Running scenario: ") + options[idxs[sc-1]].key);
                }
            }
            else
            {
                // All scenarios flat list
                std::cout << "Select a simulation to run:" << '\n';
                for (size_t i = 0; i < options.size(); ++i)
                {
                    std::cout << "  [" << (i + 1) << "] " << options[i].title << " (" << options[i].key << ")" << '\n';
                }
                size_t choice = promptChoice(options.size(), "Enter choice number (default 1): ");
                scenario = options[choice - 1].factory();
                logger.Info(std::string("Running scenario: ") + options[choice - 1].key);
            }
        }
        else
        {
            // Fallback: flat list
            std::cout << "Select a simulation to run:" << '\n';
            for (size_t i = 0; i < options.size(); ++i)
            {
                std::cout << "  [" << (i + 1) << "] " << options[i].title << " (" << options[i].key << ")" << '\n';
            }
            size_t choice = promptChoice(options.size(), "Enter choice number (default 1): ");
            scenario = options[choice - 1].factory();
            logger.Info(std::string("Running scenario: ") + options[choice - 1].key);
        }
    }
    scenario->Setup(world);

    std::atomic<bool> running{true};

    core::FixedTimestepLoop loop{1.0f / 60.0f};

    // Spawn a simple input thread that stops the simulation when Enter is pressed.
    std::thread quitThread([&running, &logger]() {
        logger.Info("Press Enter to quit...");
        std::string line;
        std::getline(std::cin, line);
        running.store(false);
    });
    quitThread.detach();

    loop.Run(
        [&](float dt)
        {
            scenario->Step(world, dt);
            world.Update(dt);

            // Runs until Enter is pressed (quitThread toggles running=false)
        },
        running);

    logger.Info("AtlasCore shutting down.");
    return 0;
}
