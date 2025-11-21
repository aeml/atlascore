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
#include "core/Clock.hpp"
#include "core/FixedTimestepLoop.hpp"
#include "ecs/World.hpp"
#include "simlab/Scenario.hpp"

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <iostream>
#include <vector>
#include <thread>
#include <filesystem>

int main(int argc, char** argv)
{
    core::Logger logger;
    logger.Info("AtlasCore starting up...");

    ecs::World world;
    const auto& options = simlab::ScenarioRegistry::All();

    auto envTruthy = [](const char* value) {
        if (!value || !*value) return false;
        switch (value[0])
        {
        case '1':
        case 'y': case 'Y':
        case 't': case 'T':
            return true;
        default:
            return false;
        }
    };

    auto getEnvValue = [](const char* key) -> std::string {
#ifdef _WIN32
        char* buffer = nullptr;
        size_t len = 0;
        if (_dupenv_s(&buffer, &len, key) == 0 && buffer)
        {
            std::string value(buffer);
            free(buffer);
            return value;
        }
        return {};
#else
        const char* value = std::getenv(key);
        return value ? std::string(value) : std::string{};
#endif
    };

    std::string headlessEnvValue = getEnvValue("ATLASCORE_HEADLESS");
    bool headless = envTruthy(headlessEnvValue.c_str());
    std::string scenarioArg;
    int maxFrames = -1; // Headless auto-termination after N frames if >0
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg{argv[i]};
        if (arg == "--headless")
        {
            headless = true;
        }
        else if (arg.rfind("--frames=", 0) == 0)
        {
            // Parse --frames=N
            auto value = std::string(arg.substr(9));
            try { maxFrames = std::stoi(value); } catch(...) { maxFrames = -1; }
            if (maxFrames < 0) {
                logger.Warn("Ignoring invalid --frames value: " + std::string(value));
            }
        }
        else if (scenarioArg.empty())
        {
            scenarioArg = std::string(arg);
        }
    }
    simlab::SetHeadlessRendering(headless);
    if (headless)
    {
        logger.Info("Headless renderer enabled");
    }

    // Determine scenario by CLI arg or interactive menu
    std::unique_ptr<simlab::IScenario> scenario;
    if (!scenarioArg.empty())
    {
        auto factory = simlab::ScenarioRegistry::FindFactory(scenarioArg);
        if (factory)
        {
            scenario = factory();
        }
        else
        {
            logger.Error(std::string("Unknown scenario: ") + scenarioArg);
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
    if (!scenario && !options.empty())
    {
        scenario = options.front().factory();
    }

    if (!scenario)
    {
        logger.Error("No scenario available to run.");
        return 1;
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

    std::ofstream headlessOut;
    if (headless)
    {
        headlessOut.open("headless_output.txt");
        if (!headlessOut.is_open())
        {
            logger.Error("Failed to open headless_output.txt");
        }
        else
        {
            try {
                auto cwd = std::filesystem::current_path();
                logger.Info(std::string("Headless output path: ") + (cwd / "headless_output.txt").string());
            } catch(...) {
                logger.Warn("Could not determine current working directory for headless output");
            }
        }
    }

    int frameCounter = 0;
    loop.Run(
        [&](float dt)
        {
            scenario->Update(world, dt);
            world.Update(dt);
            if (headless)
            {
                if (headlessOut.is_open())
                {
                    scenario->Render(world, headlessOut);
                }
            }
            else
            {
                scenario->Render(world, std::cout);
            }

            if (maxFrames > 0)
            {
                ++frameCounter;
                if (frameCounter >= maxFrames)
                {
                    running.store(false);
                }
            }
            // Otherwise runs until Enter is pressed.
        },
        running);

    logger.Info("AtlasCore shutting down.");
    return 0;
}
