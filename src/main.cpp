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
#include <ctime>
#include <stdexcept>

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
    auto classifyStartupFailure = [&](std::string_view phase, const std::string& detail = std::string{}) {
        outcome.MarkStartupFailure(phase, detail);
    };

    auto writeStartupFailureArtifacts = [&](const std::string& category) {
        simlab::HeadlessRunReportContext reportContext{};
        reportContext.requestedScenarioKey = requestedScenarioKey;
        reportContext.resolvedScenarioKey = selectedScenarioKey;
        reportContext.fallbackUsed = fallbackUsed;
        reportContext.fixedDtSeconds = fixedDtSeconds;
        reportContext.boundedFrames = boundedFrames;
        reportContext.requestedFrames = requestedFrames;
        reportContext.headless = headless;
        reportContext.runConfigHash = runConfigHash;
        reportContext.terminationReason = outcome.DeriveTerminationReason(boundedFrames,
                                                                          headless,
                                                                          false,
                                                                          false);

        const auto startupFailureArtifacts = simlab::WriteHeadlessStartupFailureArtifacts(startupFailureSummaryPath,
                                                                                          startupFailureManifestPath,
                                                                                          selectedScenarioKey,
                                                                                          reportContext,
                                                                                          outcome,
                                                                                          category,
                                                                                          formatTimestampUtc(),
                                                                                          ATLASCORE_BUILD_GIT_COMMIT,
                                                                                          ATLASCORE_BUILD_GIT_DIRTY != 0,
                                                                                          ATLASCORE_BUILD_TYPE);

        startupFailureSummaryWriteStatus = startupFailureArtifacts.manifest.startupFailureSummaryWriteStatus;
        startupFailureSummaryFailureCategory = startupFailureArtifacts.manifest.startupFailureSummaryFailureCategory;
        startupFailureManifestWriteStatus = startupFailureArtifacts.manifest.startupFailureManifestWriteStatus;
        startupFailureManifestFailureCategory = startupFailureArtifacts.manifest.startupFailureManifestFailureCategory;

        if (!startupFailureArtifacts.summaryOpened)
        {
            logger.Error(std::string("Failed to open startup failure summary: ") + startupFailureSummaryPath);
        }
        if (!startupFailureArtifacts.manifestOpened)
        {
            logger.Error(std::string("Failed to open startup failure manifest: ") + startupFailureManifestPath);
        }
    };
    try
    {
        maybeFailPhase("setup");
        scenario->Setup(world);
    }
    catch (const std::exception& ex)
    {
        classifyStartupFailure("setup", ex.what());
    }
    catch (...)
    {
        classifyStartupFailure("setup");
    }

    if (headless)
    {
        auto ensureParentDirectoryExists = [&](const std::filesystem::path& path, const std::string& label) {
            if (!path.has_parent_path())
            {
                return;
            }

            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
            {
                if (label == "output")
                {
                    classifyStartupFailure("output_directory_create_failed");
                }
                else
                {
                    batchIndexAppendStatus = "append_failed";
                    batchIndexFailureCategory = "batch_index_open_failed";
                }
                logger.Error(std::string("Failed to create ") + label + " directory: " + path.parent_path().string());
            }
        };

        const std::string outputBase = outputPrefix.empty() ? "headless" : outputPrefix;
        const std::filesystem::path outputBasePath(outputBase);
        outputPath = outputBasePath.string() + "_output.txt";
        metricsPath = outputBasePath.string() + "_metrics.csv";
        summaryPath = outputBasePath.string() + "_summary.csv";
        manifestPath = outputBasePath.string() + "_manifest.csv";
        ensureParentDirectoryExists(outputBasePath, "output");
        if (!batchIndexPath.empty())
        {
            ensureParentDirectoryExists(std::filesystem::path(batchIndexPath), "batch index");
        }

        headlessOut.open(outputPath);
        if (!headlessOut.is_open())
        {
            outcome.MarkStartupFailureCategory("output_file_open_failed");
            logger.Error(std::string("Failed to open ") + outputPath);
        }
        else
        {
            simlab::MarkHeadlessArtifactOpened(outputWriteStatus);
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
            outcome.MarkStartupFailureCategory("metrics_file_open_failed");
            logger.Error(std::string("Failed to open ") + metricsPath);
        }
        else
        {
            simlab::MarkHeadlessArtifactOpened(metricsWriteStatus);
            simlab::WriteFrameMetricsCsvHeader(headlessMetricsOut);
            simlab::FinalizeHeadlessArtifactWrite(headlessMetricsOut,
                                                 metricsWriteStatus,
                                                 metricsFailureCategory,
                                                 "metrics_write_failed");
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
            outcome.MarkStartupFailureCategory("summary_file_open_failed");
            logger.Error(std::string("Failed to open ") + summaryPath);
        }
        else
        {
            simlab::MarkHeadlessArtifactOpened(summaryWriteStatus);
            simlab::WriteHeadlessRunSummaryCsvHeader(headlessSummaryOut);
            simlab::FinalizeHeadlessArtifactWrite(headlessSummaryOut,
                                                 summaryWriteStatus,
                                                 summaryFailureCategory,
                                                 "summary_write_failed");
            try {
                auto cwd = std::filesystem::current_path();
                logger.Info(std::string("Headless summary path: ") + (cwd / summaryPath).string());
            } catch(...) {
                logger.Warn("Could not determine current working directory for headless summary");
            }
        }

        headlessManifestOut.open(manifestPath);
        if (!headlessManifestOut.is_open())
        {
            outcome.MarkStartupFailureCategory("manifest_file_open_failed");
            logger.Error(std::string("Failed to open ") + manifestPath);
        }
        else
        {
            simlab::MarkHeadlessArtifactOpened(manifestWriteStatus);
            simlab::WriteHeadlessRunManifestCsvHeader(headlessManifestOut);
            simlab::FinalizeHeadlessArtifactWrite(headlessManifestOut,
                                                 manifestWriteStatus,
                                                 manifestFailureCategory,
                                                 "manifest_write_failed");
            try {
                auto cwd = std::filesystem::current_path();
                logger.Info(std::string("Headless manifest path: ") + (cwd / manifestPath).string());
            } catch(...) {
                logger.Warn("Could not determine current working directory for headless manifest");
            }
        }
    }

    if (outcome.runStatus == "startup_failure")
    {
        writeStartupFailureArtifacts(outcome.failureCategory);
        logger.Info("AtlasCore shutting down.");
        return outcome.exitCode;
    }

    int frameCounter = 0;
    double simTimeSeconds = 0.0;
    std::string currentRuntimeFailurePhase;
    try
    {
        loop.Run(
            [&](float dt)
            {
                using steady_clock = std::chrono::steady_clock;
                const auto frameStart = steady_clock::now();
                const auto updateStart = frameStart;
                currentRuntimeFailurePhase = "update";
                maybeFailPhase("update");
                scenario->Update(world, dt);
                currentRuntimeFailurePhase = "world_update";
                maybeFailPhase("world_update");
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
                currentRuntimeFailurePhase = "render";
                maybeFailPhase("render");
                scenario->Render(world, *renderStream);
                currentRuntimeFailurePhase.clear();
                if (headless && headlessOut.is_open())
                {
                    simlab::FinalizeHeadlessArtifactWrite(headlessOut,
                                                         outputWriteStatus,
                                                         outputFailureCategory,
                                                         "output_write_failed");
                }
                const auto renderEnd = steady_clock::now();

                if (const auto* physicsSystem = world.FindSystem<physics::PhysicsSystem>())
                {
                    auto metrics = simlab::CaptureFrameMetrics(world, *physicsSystem, static_cast<std::size_t>(frameCounter), simTimeSeconds);
                    metrics.updateWallSeconds = std::chrono::duration<double>(updateEnd - updateStart).count();
                    metrics.renderWallSeconds = std::chrono::duration<double>(renderEnd - renderStart).count();
                    metrics.frameWallSeconds = std::chrono::duration<double>(renderEnd - frameStart).count();
                    metrics.frameWallSeconds = std::max(metrics.frameWallSeconds,
                                                        metrics.updateWallSeconds + metrics.renderWallSeconds);
                    headlessSummaryAccumulator.AddFrame(metrics);
                    if (headlessMetricsOut.is_open() && metricsFailureCategory.empty())
                    {
                        simlab::WriteFrameMetricsCsvRow(headlessMetricsOut, metrics);
                        simlab::FinalizeHeadlessArtifactWrite(headlessMetricsOut,
                                                             metricsWriteStatus,
                                                             metricsFailureCategory,
                                                             "metrics_write_failed");
                    }
                }

                if (maxFrames > 0 && frameCounter >= maxFrames)
                {
                    running.store(false);
                }
                else if (headless && !boundedFrames)
                {
                    running.store(false);
                }
                // Otherwise runs until Enter is pressed.
            },
            running);
    }
    catch (const std::exception& ex)
    {
        const std::string_view failurePhase = currentRuntimeFailurePhase.empty()
            ? std::string_view{failPhase}
            : std::string_view{currentRuntimeFailurePhase};
        outcome.MarkRuntimeFailure(failurePhase, ex.what());
        running.store(false);
    }

    if (quitThread.joinable())
    {
        quitThread.join();
    }

    const std::string terminationReason = outcome.DeriveTerminationReason(boundedFrames,
                                                                          headless,
                                                                          quitRequestedByInput.load(),
                                                                          quitRequestedByEof.load());

    simlab::HeadlessRunReportContext reportContext{};
    reportContext.requestedScenarioKey = requestedScenarioKey;
    reportContext.resolvedScenarioKey = selectedScenarioKey;
    reportContext.fallbackUsed = fallbackUsed;
    reportContext.fixedDtSeconds = fixedDtSeconds;
    reportContext.boundedFrames = boundedFrames;
    reportContext.requestedFrames = requestedFrames;
    reportContext.headless = headless;
    reportContext.runConfigHash = runConfigHash;
    reportContext.terminationReason = terminationReason;

    auto runSummary = simlab::BuildHeadlessRunSummaryReport(headlessSummaryAccumulator.Build(selectedScenarioKey),
                                                            reportContext,
                                                            outcome);
    if (headlessSummaryOut.is_open() && summaryFailureCategory.empty())
    {
        simlab::WriteHeadlessRunSummaryCsvRow(headlessSummaryOut, runSummary);
        simlab::FinalizeHeadlessArtifactWrite(headlessSummaryOut,
                                             summaryWriteStatus,
                                             summaryFailureCategory,
                                             "summary_write_failed");
    }

    if (headlessManifestOut.is_open())
    {
        simlab::HeadlessRunArtifactReport artifacts{};
        artifacts.outputPath = toAbsolutePathString(outputPath);
        artifacts.metricsPath = toAbsolutePathString(metricsPath);
        artifacts.summaryPath = toAbsolutePathString(summaryPath);
        artifacts.batchIndexPath = batchIndexPath.empty() ? std::string{} : toAbsolutePathString(batchIndexPath);
        artifacts.batchIndexAppendStatus = batchIndexAppendStatus;
        artifacts.batchIndexFailureCategory = batchIndexFailureCategory;
        artifacts.outputWriteStatus = outputWriteStatus;
        artifacts.outputFailureCategory = outputFailureCategory;
        artifacts.metricsWriteStatus = metricsWriteStatus;
        artifacts.metricsFailureCategory = metricsFailureCategory;
        artifacts.summaryWriteStatus = summaryWriteStatus;
        artifacts.summaryFailureCategory = summaryFailureCategory;
        artifacts.manifestWriteStatus = manifestWriteStatus;
        artifacts.manifestFailureCategory = manifestFailureCategory;
        artifacts.startupFailureSummaryWriteStatus = startupFailureSummaryWriteStatus;
        artifacts.startupFailureSummaryFailureCategory = startupFailureSummaryFailureCategory;
        artifacts.startupFailureManifestWriteStatus = startupFailureManifestWriteStatus;
        artifacts.startupFailureManifestFailureCategory = startupFailureManifestFailureCategory;
        artifacts.timestampUtc = formatTimestampUtc();
        artifacts.gitCommit = ATLASCORE_BUILD_GIT_COMMIT;
        artifacts.gitDirty = ATLASCORE_BUILD_GIT_DIRTY != 0;
        artifacts.buildType = ATLASCORE_BUILD_TYPE;

        auto manifest = simlab::BuildHeadlessRunManifestReport(runSummary.frameCount,
                                                               reportContext,
                                                               artifacts,
                                                               outcome);

        if (!batchIndexPath.empty())
        {
            const auto absoluteBatchIndexPath = toAbsolutePathString(batchIndexPath);
            simlab::AppendHeadlessManifestToBatchIndex(absoluteBatchIndexPath,
                                                       manifest,
                                                       batchIndexAppendStatus,
                                                       batchIndexFailureCategory);
            manifest.batchIndexAppendStatus = batchIndexAppendStatus;
            manifest.batchIndexFailureCategory = batchIndexFailureCategory;
            if (batchIndexFailureCategory == "batch_index_open_failed")
            {
                logger.Error(std::string("Failed to open batch index: ") + absoluteBatchIndexPath);
            }
        }

        if (manifestFailureCategory.empty())
        {
            simlab::WriteHeadlessRunManifestCsvRow(headlessManifestOut, manifest);
            simlab::FinalizeHeadlessArtifactWrite(headlessManifestOut,
                                                 manifestWriteStatus,
                                                 manifestFailureCategory,
                                                 "manifest_write_failed");
        }
    }

    logger.Info("AtlasCore shutting down.");
    return outcome.exitCode;
}
