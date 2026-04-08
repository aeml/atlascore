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
#include "simlab/HeadlessMetrics.hpp"
#include "physics/Systems.hpp"

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <iostream>
#include <vector>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <chrono>

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
    std::string outputPrefix;
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
        else if (arg.rfind("--output-prefix=", 0) == 0)
        {
            outputPrefix = std::string(arg.substr(16));
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
    std::string selectedScenarioKey;
    if (!scenarioArg.empty())
    {
        auto factory = simlab::ScenarioRegistry::FindFactory(scenarioArg);
        if (factory)
        {
            scenario = factory();
            selectedScenarioKey = scenarioArg;
        }
        else
        {
            logger.Error(std::string("Unknown scenario: ") + scenarioArg);
            if (!options.empty())
            {
                scenario = options.front().factory();
                selectedScenarioKey = options.front().key;
            }
        }
    }
    else
    {
        // Simple flat menu
        std::cout << "========================================\n";
        std::cout << "      AtlasCore Simulation Framework    \n";
        std::cout << "========================================\n";
        std::cout << "Select a simulation to run:\n";
        
        for (size_t i = 0; i < options.size(); ++i)
        {
            std::cout << "  [" << (i + 1) << "] " << options[i].title << " (" << options[i].key << ")\n";
        }
        
        std::cout << "\nEnter choice number (default 1): ";
        std::string line;
        std::getline(std::cin, line);
        
        size_t choice = 1;
        if (!line.empty())
        {
            try {
                unsigned long p = std::stoul(line);
                if (p >= 1 && p <= options.size())
                {
                    choice = static_cast<size_t>(p);
                }
            } catch (...) {}
        }

        if (!options.empty())
        {
            const auto& selected = options[choice - 1];
            scenario = selected.factory();
            selectedScenarioKey = selected.key;
            logger.Info(std::string("Running scenario: ") + selected.key);
        }
    }
    if (!scenario && !options.empty())
    {
        scenario = options.front().factory();
        selectedScenarioKey = options.front().key;
    }

    if (!scenario)
    {
        logger.Error("No scenario available to run.");
        return 1;
    }

    scenario->Setup(world);

    std::atomic<bool> running{true};

    core::FixedTimestepLoop loop{1.0f / 60.0f};

    std::thread quitThread;
    const bool useInputQuitThread = !headless && maxFrames <= 0;
    if (useInputQuitThread)
    {
        // Stop interactive simulations when Enter is pressed.
        quitThread = std::thread([&running, &logger]() {
            logger.Info("Press Enter to quit...");
            std::string line;
            std::getline(std::cin, line);
            running.store(false);
        });
    }

    std::ofstream headlessOut;
    std::ofstream headlessMetricsOut;
    std::ofstream headlessSummaryOut;
    simlab::HeadlessRunSummaryAccumulator headlessSummaryAccumulator;
    if (headless)
    {
        const std::string outputBase = outputPrefix.empty() ? "headless" : outputPrefix;
        const std::filesystem::path outputBasePath(outputBase);
        const auto outputPath = outputBasePath.string() + "_output.txt";
        const auto metricsPath = outputBasePath.string() + "_metrics.csv";
        const auto summaryPath = outputBasePath.string() + "_summary.csv";
        if (outputBasePath.has_parent_path())
        {
            std::error_code ec;
            std::filesystem::create_directories(outputBasePath.parent_path(), ec);
            if (ec)
            {
                logger.Error(std::string("Failed to create output directory: ") + outputBasePath.parent_path().string());
            }
        }

        headlessOut.open(outputPath);
        if (!headlessOut.is_open())
        {
            logger.Error(std::string("Failed to open ") + outputPath);
        }
        else
        {
            try {
                auto cwd = std::filesystem::current_path();
                logger.Info(std::string("Headless output path: ") + (cwd / outputPath).string());
            } catch(...) {
                logger.Warn("Could not determine current working directory for headless output");
            }
        }

        headlessMetricsOut.open(metricsPath);
        if (!headlessMetricsOut.is_open())
        {
            logger.Error(std::string("Failed to open ") + metricsPath);
        }
        else
        {
            simlab::WriteFrameMetricsCsvHeader(headlessMetricsOut);
            try {
                auto cwd = std::filesystem::current_path();
                logger.Info(std::string("Headless metrics path: ") + (cwd / metricsPath).string());
            } catch(...) {
                logger.Warn("Could not determine current working directory for headless metrics");
            }
        }

        headlessSummaryOut.open(summaryPath);
        if (!headlessSummaryOut.is_open())
        {
            logger.Error(std::string("Failed to open ") + summaryPath);
        }
        else
        {
            simlab::WriteHeadlessRunSummaryCsvHeader(headlessSummaryOut);
            try {
                auto cwd = std::filesystem::current_path();
                logger.Info(std::string("Headless summary path: ") + (cwd / summaryPath).string());
            } catch(...) {
                logger.Warn("Could not determine current working directory for headless summary");
            }
        }
    }

    int frameCounter = 0;
    double simTimeSeconds = 0.0;
    loop.Run(
        [&](float dt)
        {
            using steady_clock = std::chrono::steady_clock;
            const auto frameStart = steady_clock::now();
            const auto updateStart = frameStart;
            scenario->Update(world, dt);
            world.Update(dt);
            const auto updateEnd = steady_clock::now();
            simTimeSeconds += static_cast<double>(dt);
            ++frameCounter;

            std::ostream* renderStream = &std::cout;
            if (headless && headlessOut.is_open())
            {
                renderStream = &headlessOut;
            }

            const auto renderStart = steady_clock::now();
            scenario->Render(world, *renderStream);
            const auto renderEnd = steady_clock::now();

            if (headlessMetricsOut.is_open())
            {
                if (const auto* physicsSystem = world.FindSystem<physics::PhysicsSystem>())
                {
                    auto metrics = simlab::CaptureFrameMetrics(world, *physicsSystem, static_cast<std::size_t>(frameCounter), simTimeSeconds);
                    metrics.updateWallSeconds = std::chrono::duration<double>(updateEnd - updateStart).count();
                    metrics.renderWallSeconds = std::chrono::duration<double>(renderEnd - renderStart).count();
                    metrics.frameWallSeconds = std::chrono::duration<double>(renderEnd - frameStart).count();
                    metrics.frameWallSeconds = std::max(metrics.frameWallSeconds,
                                                        metrics.updateWallSeconds + metrics.renderWallSeconds);
                    headlessSummaryAccumulator.AddFrame(metrics);
                    simlab::WriteFrameMetricsCsvRow(headlessMetricsOut, metrics);
                }
            }

            if (maxFrames > 0 && frameCounter >= maxFrames)
            {
                running.store(false);
            }
            // Otherwise runs until Enter is pressed.
        },
        running);

    if (quitThread.joinable())
    {
        quitThread.join();
    }

    if (headlessSummaryOut.is_open())
    {
        simlab::WriteHeadlessRunSummaryCsvRow(headlessSummaryOut,
                                             headlessSummaryAccumulator.Build(selectedScenarioKey));
    }

    logger.Info("AtlasCore shutting down.");
    return 0;
}
