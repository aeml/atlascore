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
#include <ctime>
#include <stdexcept>
#include <utility>

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
    const std::string failPhase = getEnvValue("ATLASCORE_FAIL_PHASE");
    auto maybeFailPhase = [&](std::string_view phase) {
        if (failPhase == phase)
        {
            throw std::runtime_error(std::string("Injected failure for phase: ") + std::string(phase));
        }
    };
    bool headless = envTruthy(headlessEnvValue.c_str());
    std::string scenarioArg;
    std::string outputPrefix;
    std::string batchIndexPath;
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
        else if (arg.rfind("--batch-index=", 0) == 0)
        {
            batchIndexPath = std::string(arg.substr(14));
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
    const std::string requestedScenarioKey = scenarioArg.empty() ? (options.empty() ? std::string{} : options.front().key) : scenarioArg;
    bool fallbackUsed = false;
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
                fallbackUsed = true;
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

    std::atomic<bool> running{true};
    std::atomic<bool> quitRequestedByInput{false};
    std::atomic<bool> quitRequestedByEof{false};

    constexpr double fixedDtSeconds = 1.0 / 60.0;
    const bool boundedFrames = maxFrames > 0;
    const auto requestedFrames = boundedFrames ? static_cast<std::size_t>(maxFrames) : 0u;
    const auto runConfigHash = simlab::HashHeadlessRunConfig(selectedScenarioKey,
                                                             fixedDtSeconds,
                                                             requestedFrames,
                                                             headless);

    auto formatTimestampUtc = []() -> std::string {
        const auto now = std::chrono::system_clock::now();
        const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm utcTime{};
#if defined(_WIN32)
        gmtime_s(&utcTime, &nowTime);
#else
        gmtime_r(&nowTime, &utcTime);
#endif
        char buffer[32]{};
        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime) == 0)
        {
            return {};
        }
        return std::string(buffer);
    };

    auto toAbsolutePathString = [](const std::string& pathText) -> std::string {
        std::error_code ec;
        const auto absolutePath = std::filesystem::absolute(std::filesystem::path(pathText), ec);
        return ec ? pathText : absolutePath.string();
    };

    core::FixedTimestepLoop loop{static_cast<float>(fixedDtSeconds)};

    std::thread quitThread;
    const bool useInputQuitThread = !headless && maxFrames <= 0;
    if (useInputQuitThread)
    {
        // Stop interactive simulations when Enter is pressed.
        quitThread = std::thread([&running, &quitRequestedByInput, &quitRequestedByEof, &logger]() {
            logger.Info("Press Enter to quit...");
            std::string line;
            if (std::getline(std::cin, line))
            {
                quitRequestedByInput.store(true);
            }
            else
            {
                quitRequestedByEof.store(true);
            }
            running.store(false);
        });
    }

    std::ofstream headlessOut;
    std::ofstream headlessMetricsOut;
    std::ofstream headlessSummaryOut;
    std::ofstream headlessManifestOut;
    simlab::HeadlessRunSummaryAccumulator headlessSummaryAccumulator;
    std::string outputPath;
    std::string metricsPath;
    std::string summaryPath;
    std::string manifestPath;
    simlab::HeadlessRunOutcomeTracker outcome;
    std::string batchIndexAppendStatus = batchIndexPath.empty() ? "not_requested" : "appended";
    std::string batchIndexFailureCategory;
    std::string outputWriteStatus;
    std::string outputFailureCategory;
    std::string metricsWriteStatus;
    std::string metricsFailureCategory;
    std::string summaryWriteStatus;
    std::string summaryFailureCategory;
    std::string manifestWriteStatus;
    std::string manifestFailureCategory;
    std::string startupFailureSummaryWriteStatus{"not_applicable"};
    std::string startupFailureSummaryFailureCategory;
    std::string startupFailureManifestWriteStatus{"not_applicable"};
    std::string startupFailureManifestFailureCategory;
    const std::string startupFailureSummaryPath = "headless_startup_failure_summary.csv";
    const std::string startupFailureManifestPath = "headless_startup_failure_manifest.csv";
    const auto startupConfig = simlab::BuildHeadlessStartupCoordinatorConfig(
        headless,
        outputPrefix,
        batchIndexPath,
        requestedScenarioKey,
        selectedScenarioKey,
        fallbackUsed,
        fixedDtSeconds,
        boundedFrames,
        requestedFrames,
        runConfigHash,
        startupFailureSummaryPath,
        startupFailureManifestPath,
        formatTimestampUtc(),
        ATLASCORE_BUILD_GIT_COMMIT,
        ATLASCORE_BUILD_GIT_DIRTY != 0,
        ATLASCORE_BUILD_TYPE);

    auto startup = simlab::CoordinateHeadlessStartup(world,
                                                     *scenario,
                                                     outcome,
                                                     startupConfig,
                                                     maybeFailPhase);

    outputPath = startup.bootstrap.outputPath;
    metricsPath = startup.bootstrap.metricsPath;
    summaryPath = startup.bootstrap.summaryPath;
    manifestPath = startup.bootstrap.manifestPath;
    batchIndexAppendStatus = startup.bootstrap.batchIndexAppendStatus;
    batchIndexFailureCategory = startup.bootstrap.batchIndexFailureCategory;
    outputWriteStatus = startup.bootstrap.outputWriteStatus;
    outputFailureCategory = startup.bootstrap.outputFailureCategory;
    metricsWriteStatus = startup.bootstrap.metricsWriteStatus;
    metricsFailureCategory = startup.bootstrap.metricsFailureCategory;
    summaryWriteStatus = startup.bootstrap.summaryWriteStatus;
    summaryFailureCategory = startup.bootstrap.summaryFailureCategory;
    manifestWriteStatus = startup.bootstrap.manifestWriteStatus;
    manifestFailureCategory = startup.bootstrap.manifestFailureCategory;
    startupFailureSummaryWriteStatus = startup.startupFailureSummaryWriteStatus;
    startupFailureSummaryFailureCategory = startup.startupFailureSummaryFailureCategory;
    startupFailureManifestWriteStatus = startup.startupFailureManifestWriteStatus;
    startupFailureManifestFailureCategory = startup.startupFailureManifestFailureCategory;
    headlessOut = std::move(startup.bootstrap.outputStream);
    headlessMetricsOut = std::move(startup.bootstrap.metricsStream);
    headlessSummaryOut = std::move(startup.bootstrap.summaryStream);
    headlessManifestOut = std::move(startup.bootstrap.manifestStream);

    simlab::LogHeadlessStartupMessages(logger,
                                      startup,
                                      outcome,
                                      outputPath,
                                      metricsPath,
                                      summaryPath,
                                      manifestPath,
                                      batchIndexPath,
                                      startupFailureSummaryPath,
                                      startupFailureManifestPath);

    if (outcome.runStatus == "startup_failure")
    {
        logger.Info("AtlasCore shutting down.");
        return outcome.exitCode;
    }

    simlab::HeadlessRuntimeFrameState runtimeFrameState{};
    simlab::HeadlessRuntimeFrameConfig runtimeFrameConfig{};
    runtimeFrameConfig.headless = headless;
    runtimeFrameConfig.boundedFrames = boundedFrames;
    runtimeFrameConfig.maxFrames = maxFrames;
    simlab::HeadlessRuntimeFrameArtifacts runtimeFrameArtifacts{};
    runtimeFrameArtifacts.outputStream = headless && headlessOut.is_open() ? static_cast<std::ostream*>(&headlessOut) : nullptr;
    runtimeFrameArtifacts.metricsStream = headless && headlessMetricsOut.is_open() ? static_cast<std::ostream*>(&headlessMetricsOut) : nullptr;
    runtimeFrameArtifacts.outputWriteStatus = &outputWriteStatus;
    runtimeFrameArtifacts.outputFailureCategory = &outputFailureCategory;
    runtimeFrameArtifacts.metricsWriteStatus = &metricsWriteStatus;
    runtimeFrameArtifacts.metricsFailureCategory = &metricsFailureCategory;
    try
    {
        loop.Run(
            [&](float dt)
            {
                const bool shouldStop = simlab::RunHeadlessRuntimeFrame(world,
                                                                        *scenario,
                                                                        dt,
                                                                        runtimeFrameState,
                                                                        runtimeFrameConfig,
                                                                        headlessSummaryAccumulator,
                                                                        std::cout,
                                                                        runtimeFrameArtifacts,
                                                                        maybeFailPhase);
                if (shouldStop)
                {
                    running.store(false);
                }
                // Otherwise runs until Enter is pressed.
            },
            running);
    }
    catch (const std::exception& ex)
    {
        const std::string_view failurePhase = runtimeFrameState.currentFailurePhase.empty()
            ? std::string_view{failPhase}
            : std::string_view{runtimeFrameState.currentFailurePhase};
        outcome.MarkRuntimeFailure(failurePhase, ex.what());
        running.store(false);
    }

    if (quitThread.joinable())
    {
        quitThread.join();
    }

    const auto finalizationPreparation = simlab::PrepareHeadlessRunFinalization(
        outcome,
        requestedScenarioKey,
        selectedScenarioKey,
        fallbackUsed,
        fixedDtSeconds,
        boundedFrames,
        requestedFrames,
        headless,
        runConfigHash,
        quitRequestedByInput.load(),
        quitRequestedByEof.load(),
        toAbsolutePathString(outputPath),
        toAbsolutePathString(metricsPath),
        toAbsolutePathString(summaryPath),
        batchIndexPath.empty() ? std::string{} : toAbsolutePathString(batchIndexPath),
        batchIndexAppendStatus,
        batchIndexFailureCategory,
        outputWriteStatus,
        outputFailureCategory,
        metricsWriteStatus,
        metricsFailureCategory,
        summaryWriteStatus,
        summaryFailureCategory,
        manifestWriteStatus,
        manifestFailureCategory,
        startupFailureSummaryWriteStatus,
        startupFailureSummaryFailureCategory,
        startupFailureManifestWriteStatus,
        startupFailureManifestFailureCategory,
        formatTimestampUtc(),
        ATLASCORE_BUILD_GIT_COMMIT,
        ATLASCORE_BUILD_GIT_DIRTY != 0,
        ATLASCORE_BUILD_TYPE);

    const auto finalization = simlab::FinalizeHeadlessRunReports(selectedScenarioKey,
                                                                 headlessSummaryAccumulator,
                                                                 finalizationPreparation.context,
                                                                 outcome,
                                                                 finalizationPreparation.artifacts,
                                                                 headlessSummaryOut.is_open() ? static_cast<std::ostream*>(&headlessSummaryOut) : nullptr,
                                                                 headlessManifestOut.is_open() ? static_cast<std::ostream*>(&headlessManifestOut) : nullptr);

    summaryWriteStatus = finalization.artifacts.summaryWriteStatus;
    summaryFailureCategory = finalization.artifacts.summaryFailureCategory;
    manifestWriteStatus = finalization.artifacts.manifestWriteStatus;
    manifestFailureCategory = finalization.artifacts.manifestFailureCategory;
    batchIndexAppendStatus = finalization.artifacts.batchIndexAppendStatus;
    batchIndexFailureCategory = finalization.artifacts.batchIndexFailureCategory;

    simlab::LogHeadlessFinalizationMessages(logger,
                                            batchIndexPath.empty() ? std::string{} : toAbsolutePathString(batchIndexPath),
                                            batchIndexFailureCategory);

    logger.Info("AtlasCore shutting down.");
    return outcome.exitCode;
}
