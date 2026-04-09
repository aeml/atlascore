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

#include "simlab/HeadlessMetrics.hpp"

#include "ecs/World.hpp"
#include "physics/Systems.hpp"
#include "simlab/Scenario.hpp"
#include "simlab/WorldHasher.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <system_error>
#include <vector>

namespace
{
    constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;

    double AverageOrZero(const double total, const std::size_t count) noexcept
    {
        return count > 0 ? total / static_cast<double>(count) : 0.0;
    }

    double NearestRankPercentile(std::vector<double> samples, const double percentile) noexcept
    {
        if (samples.empty())
        {
            return 0.0;
        }

        std::sort(samples.begin(), samples.end());
        const double rank = std::ceil((percentile / 100.0) * static_cast<double>(samples.size()));
        const std::size_t index = std::min(samples.size() - 1,
                                           static_cast<std::size_t>(std::max(1.0, rank) - 1.0));
        return samples[index];
    }

    void HashBytes(std::uint64_t& hash, const void* data, const std::size_t size) noexcept
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < size; ++i)
        {
            hash ^= static_cast<std::uint64_t>(bytes[i]);
            hash *= kFnvPrime;
        }
    }

    void HashString(std::uint64_t& hash, const std::string& value) noexcept
    {
        HashBytes(hash, value.data(), value.size());
    }

    template <typename TValue>
    void HashValue(std::uint64_t& hash, const TValue& value) noexcept
    {
        HashBytes(hash, &value, sizeof(TValue));
    }
}

namespace simlab
{
    void HeadlessRunOutcomeTracker::MarkStartupFailure(const std::string_view phase, const std::string_view detail) noexcept
    {
        if (runStatus != "startup_failure")
        {
            runStatus = "startup_failure";
            failureCategory = std::string(ClassifyHeadlessFailurePhase(phase, true));
            failureDetail = std::string(detail);
            exitCode = 1;
            exitClassification = "startup_failure_exit";
        }
    }

    void HeadlessRunOutcomeTracker::MarkStartupFailureCategory(const std::string_view category, const std::string_view detail) noexcept
    {
        if (runStatus != "startup_failure")
        {
            runStatus = "startup_failure";
            failureCategory = std::string(category);
            failureDetail = std::string(detail);
            exitCode = 1;
            exitClassification = "startup_failure_exit";
        }
    }

    void HeadlessRunOutcomeTracker::MarkRuntimeFailure(const std::string_view phase, const std::string_view detail) noexcept
    {
        runStatus = "runtime_failure";
        failureCategory = std::string(ClassifyHeadlessFailurePhase(phase, false));
        failureDetail = std::string(detail);
        exitCode = 1;
        exitClassification = "runtime_failure_exit";
    }

    std::string HeadlessRunOutcomeTracker::DeriveTerminationReason(const bool boundedFrames,
                                                                  const bool headless,
                                                                  const bool quitRequestedByInput,
                                                                  const bool quitRequestedByEof) const
    {
        if (runStatus == "startup_failure")
        {
            return "startup_failure";
        }
        if (runStatus == "runtime_failure")
        {
            return "runtime_failure";
        }
        if (boundedFrames)
        {
            return "frame_cap";
        }
        if (headless)
        {
            return "unbounded_headless_default";
        }
        if (quitRequestedByEof)
        {
            return "eof_quit";
        }
        if (quitRequestedByInput)
        {
            return "user_quit";
        }
        return {};
    }

    HeadlessRunSummary BuildHeadlessRunSummaryReport(const HeadlessRunSummary& aggregate,
                                                     const HeadlessRunReportContext& context,
                                                     const HeadlessRunOutcomeTracker& outcome)
    {
        auto summary = aggregate;
        summary.requestedScenarioKey = context.requestedScenarioKey;
        summary.resolvedScenarioKey = context.resolvedScenarioKey;
        summary.fallbackUsed = context.fallbackUsed;
        summary.fixedDtSeconds = context.fixedDtSeconds;
        summary.boundedFrames = context.boundedFrames;
        summary.requestedFrames = context.requestedFrames;
        summary.headless = context.headless;
        summary.runConfigHash = context.runConfigHash;
        summary.runStatus = outcome.runStatus;
        summary.failureCategory = outcome.failureCategory;
        summary.failureDetail = outcome.failureDetail;
        summary.terminationReason = context.terminationReason;
        return summary;
    }

    HeadlessRunManifest BuildHeadlessRunManifestReport(const std::size_t frameCount,
                                                       const HeadlessRunReportContext& context,
                                                       const HeadlessRunArtifactReport& artifacts,
                                                       const HeadlessRunOutcomeTracker& outcome)
    {
        HeadlessRunManifest manifest{};
        manifest.scenarioKey = context.resolvedScenarioKey;
        manifest.requestedScenarioKey = context.requestedScenarioKey;
        manifest.resolvedScenarioKey = context.resolvedScenarioKey;
        manifest.fallbackUsed = context.fallbackUsed;
        manifest.fixedDtSeconds = context.fixedDtSeconds;
        manifest.boundedFrames = context.boundedFrames;
        manifest.requestedFrames = context.requestedFrames;
        manifest.headless = context.headless;
        manifest.runConfigHash = context.runConfigHash;
        manifest.frameCount = frameCount;
        manifest.runStatus = outcome.runStatus;
        manifest.failureCategory = outcome.failureCategory;
        manifest.failureDetail = outcome.failureDetail;
        manifest.terminationReason = context.terminationReason;
        manifest.outputPath = artifacts.outputPath;
        manifest.metricsPath = artifacts.metricsPath;
        manifest.summaryPath = artifacts.summaryPath;
        manifest.batchIndexPath = artifacts.batchIndexPath;
        manifest.batchIndexAppendStatus = artifacts.batchIndexAppendStatus;
        manifest.batchIndexFailureCategory = artifacts.batchIndexFailureCategory;
        manifest.outputWriteStatus = artifacts.outputWriteStatus;
        manifest.outputFailureCategory = artifacts.outputFailureCategory;
        manifest.metricsWriteStatus = artifacts.metricsWriteStatus;
        manifest.metricsFailureCategory = artifacts.metricsFailureCategory;
        manifest.summaryWriteStatus = artifacts.summaryWriteStatus;
        manifest.summaryFailureCategory = artifacts.summaryFailureCategory;
        manifest.manifestWriteStatus = artifacts.manifestWriteStatus;
        manifest.manifestFailureCategory = artifacts.manifestFailureCategory;
        manifest.startupFailureSummaryWriteStatus = artifacts.startupFailureSummaryWriteStatus;
        manifest.startupFailureSummaryFailureCategory = artifacts.startupFailureSummaryFailureCategory;
        manifest.startupFailureManifestWriteStatus = artifacts.startupFailureManifestWriteStatus;
        manifest.startupFailureManifestFailureCategory = artifacts.startupFailureManifestFailureCategory;
        manifest.exitCode = outcome.exitCode;
        manifest.exitClassification = outcome.exitClassification;
        manifest.timestampUtc = artifacts.timestampUtc;
        manifest.gitCommit = artifacts.gitCommit;
        manifest.gitDirty = artifacts.gitDirty;
        manifest.buildType = artifacts.buildType;
        return manifest;
    }

    void MarkHeadlessArtifactOpened(std::string& writeStatus)
    {
        writeStatus = "written";
    }

    void FinalizeHeadlessArtifactWrite(std::ostream& out,
                                       std::string& writeStatus,
                                       std::string& failureCategory,
                                       const std::string_view writeFailureCategory)
    {
        out.flush();
        if (!out.good())
        {
            writeStatus = "write_failed";
            failureCategory = std::string(writeFailureCategory);
        }
    }

    void FinalizeHeadlessBatchAppend(std::ostream& out,
                                     std::string& appendStatus,
                                     std::string& failureCategory,
                                     const std::string_view writeFailureCategory)
    {
        out.flush();
        if (!out.good())
        {
            appendStatus = "append_failed";
            failureCategory = std::string(writeFailureCategory);
        }
    }

    void AppendHeadlessManifestToBatchIndex(const std::filesystem::path& batchIndexPath,
                                            const HeadlessRunManifest& manifest,
                                            std::string& appendStatus,
                                            std::string& failureCategory)
    {
        std::error_code ec;
        const bool needsHeader = !std::filesystem::exists(batchIndexPath, ec)
                              || (!ec && std::filesystem::is_regular_file(batchIndexPath, ec)
                                  && std::filesystem::file_size(batchIndexPath, ec) == 0);

        std::ofstream batchIndexOut(batchIndexPath, std::ios::app);
        if (!batchIndexOut.is_open())
        {
            appendStatus = "append_failed";
            failureCategory = "batch_index_open_failed";
            return;
        }

        if (needsHeader)
        {
            WriteHeadlessRunManifestCsvHeader(batchIndexOut);
            FinalizeHeadlessBatchAppend(batchIndexOut,
                                        appendStatus,
                                        failureCategory,
                                        "batch_index_write_failed");
        }
        if (!failureCategory.empty())
        {
            return;
        }

        WriteHeadlessRunManifestCsvRow(batchIndexOut, manifest);
        FinalizeHeadlessBatchAppend(batchIndexOut,
                                    appendStatus,
                                    failureCategory,
                                    "batch_index_write_failed");
    }

    HeadlessArtifactBootstrapResult BootstrapHeadlessArtifacts(const std::filesystem::path& outputBasePath,
                                                               const std::filesystem::path& batchIndexPath)
    {
        HeadlessArtifactBootstrapResult result{};
        result.outputPath = outputBasePath.string() + "_output.txt";
        result.metricsPath = outputBasePath.string() + "_metrics.csv";
        result.summaryPath = outputBasePath.string() + "_summary.csv";
        result.manifestPath = outputBasePath.string() + "_manifest.csv";
        result.batchIndexAppendStatus = batchIndexPath.empty() ? "not_requested" : "appended";

        auto ensureParentDirectoryExists = [&](const std::filesystem::path& path, const bool outputArtifact) {
            if (!path.has_parent_path())
            {
                return true;
            }

            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (!ec)
            {
                return true;
            }

            if (outputArtifact)
            {
                result.startupFailureCategory = "output_directory_create_failed";
            }
            else
            {
                result.batchIndexAppendStatus = "append_failed";
                result.batchIndexFailureCategory = "batch_index_open_failed";
            }
            return false;
        };

        if (!ensureParentDirectoryExists(outputBasePath, true))
        {
            return result;
        }
        if (!batchIndexPath.empty())
        {
            ensureParentDirectoryExists(batchIndexPath, false);
        }

        result.outputStream.open(result.outputPath);
        if (!result.outputStream.is_open())
        {
            result.startupFailureCategory = "output_file_open_failed";
            return result;
        }
        MarkHeadlessArtifactOpened(result.outputWriteStatus);

        result.metricsStream.open(result.metricsPath);
        if (!result.metricsStream.is_open())
        {
            result.startupFailureCategory = "metrics_file_open_failed";
            return result;
        }
        MarkHeadlessArtifactOpened(result.metricsWriteStatus);
        WriteFrameMetricsCsvHeader(result.metricsStream);
        FinalizeHeadlessArtifactWrite(result.metricsStream,
                                      result.metricsWriteStatus,
                                      result.metricsFailureCategory,
                                      "metrics_write_failed");

        result.summaryStream.open(result.summaryPath);
        if (!result.summaryStream.is_open())
        {
            result.startupFailureCategory = "summary_file_open_failed";
            return result;
        }
        MarkHeadlessArtifactOpened(result.summaryWriteStatus);
        WriteHeadlessRunSummaryCsvHeader(result.summaryStream);
        FinalizeHeadlessArtifactWrite(result.summaryStream,
                                      result.summaryWriteStatus,
                                      result.summaryFailureCategory,
                                      "summary_write_failed");

        result.manifestStream.open(result.manifestPath);
        if (!result.manifestStream.is_open())
        {
            result.startupFailureCategory = "manifest_file_open_failed";
            return result;
        }
        MarkHeadlessArtifactOpened(result.manifestWriteStatus);
        WriteHeadlessRunManifestCsvHeader(result.manifestStream);
        FinalizeHeadlessArtifactWrite(result.manifestStream,
                                      result.manifestWriteStatus,
                                      result.manifestFailureCategory,
                                      "manifest_write_failed");

        return result;
    }

    bool RunHeadlessRuntimeFrame(ecs::World& world,
                                 IScenario& scenario,
                                 const float dt,
                                 HeadlessRuntimeFrameState& state,
                                 const HeadlessRuntimeFrameConfig& config,
                                 HeadlessRunSummaryAccumulator& accumulator,
                                 std::ostream& interactiveOut,
                                 const HeadlessRuntimeFrameArtifacts& artifacts,
                                 const std::function<void(std::string_view)>& maybeFailPhase)
    {
        using steady_clock = std::chrono::steady_clock;

        const auto frameStart = steady_clock::now();
        const auto updateStart = frameStart;
        state.currentFailurePhase = "update";
        maybeFailPhase("update");
        scenario.Update(world, dt);
        state.currentFailurePhase = "world_update";
        maybeFailPhase("world_update");
        world.Update(dt);
        const auto updateEnd = steady_clock::now();
        state.simTimeSeconds += static_cast<double>(dt);
        ++state.frameCounter;

        std::ostream* renderStream = &interactiveOut;
        if (config.headless && artifacts.outputStream != nullptr)
        {
            renderStream = artifacts.outputStream;
        }

        const auto renderStart = steady_clock::now();
        state.currentFailurePhase = "render";
        maybeFailPhase("render");
        scenario.Render(world, *renderStream);
        state.currentFailurePhase.clear();
        if (config.headless && artifacts.outputStream != nullptr
            && artifacts.outputWriteStatus != nullptr
            && artifacts.outputFailureCategory != nullptr)
        {
            FinalizeHeadlessArtifactWrite(*artifacts.outputStream,
                                          *artifacts.outputWriteStatus,
                                          *artifacts.outputFailureCategory,
                                          "output_write_failed");
        }
        const auto renderEnd = steady_clock::now();

        if (const auto* physicsSystem = world.FindSystem<physics::PhysicsSystem>())
        {
            auto metrics = CaptureFrameMetrics(world,
                                               *physicsSystem,
                                               static_cast<std::size_t>(state.frameCounter),
                                               state.simTimeSeconds);
            metrics.updateWallSeconds = std::chrono::duration<double>(updateEnd - updateStart).count();
            metrics.renderWallSeconds = std::chrono::duration<double>(renderEnd - renderStart).count();
            metrics.frameWallSeconds = std::chrono::duration<double>(renderEnd - frameStart).count();
            metrics.frameWallSeconds = std::max(metrics.frameWallSeconds,
                                                metrics.updateWallSeconds + metrics.renderWallSeconds);
            accumulator.AddFrame(metrics);
            if (artifacts.metricsStream != nullptr
                && artifacts.metricsWriteStatus != nullptr
                && artifacts.metricsFailureCategory != nullptr
                && artifacts.metricsFailureCategory->empty())
            {
                WriteFrameMetricsCsvRow(*artifacts.metricsStream, metrics);
                FinalizeHeadlessArtifactWrite(*artifacts.metricsStream,
                                              *artifacts.metricsWriteStatus,
                                              *artifacts.metricsFailureCategory,
                                              "metrics_write_failed");
            }
        }

        if (config.maxFrames > 0 && state.frameCounter >= config.maxFrames)
        {
            return true;
        }
        if (config.headless && !config.boundedFrames)
        {
            return true;
        }
        return false;
    }

    HeadlessRunArtifactReport BuildNormalHeadlessArtifactReport(const HeadlessRunArtifactReport& base,
                                                                const std::string_view outputPath,
                                                                const std::string_view metricsPath,
                                                                const std::string_view summaryPath,
                                                                const std::string_view batchIndexPath,
                                                                const std::string_view batchIndexAppendStatus,
                                                                const std::string_view batchIndexFailureCategory,
                                                                const std::string_view outputWriteStatus,
                                                                const std::string_view outputFailureCategory,
                                                                const std::string_view metricsWriteStatus,
                                                                const std::string_view metricsFailureCategory,
                                                                const std::string_view summaryWriteStatus,
                                                                const std::string_view summaryFailureCategory,
                                                                const std::string_view manifestWriteStatus,
                                                                const std::string_view manifestFailureCategory,
                                                                const std::string_view startupFailureSummaryWriteStatus,
                                                                const std::string_view startupFailureSummaryFailureCategory,
                                                                const std::string_view startupFailureManifestWriteStatus,
                                                                const std::string_view startupFailureManifestFailureCategory,
                                                                const std::string_view timestampUtc,
                                                                const std::string_view gitCommit,
                                                                const bool gitDirty,
                                                                const std::string_view buildType)
    {
        auto artifacts = base;
        artifacts.outputPath = std::string(outputPath);
        artifacts.metricsPath = std::string(metricsPath);
        artifacts.summaryPath = std::string(summaryPath);
        artifacts.batchIndexPath = std::string(batchIndexPath);
        artifacts.batchIndexAppendStatus = std::string(batchIndexAppendStatus);
        artifacts.batchIndexFailureCategory = std::string(batchIndexFailureCategory);
        artifacts.outputWriteStatus = std::string(outputWriteStatus);
        artifacts.outputFailureCategory = std::string(outputFailureCategory);
        artifacts.metricsWriteStatus = std::string(metricsWriteStatus);
        artifacts.metricsFailureCategory = std::string(metricsFailureCategory);
        artifacts.summaryWriteStatus = std::string(summaryWriteStatus);
        artifacts.summaryFailureCategory = std::string(summaryFailureCategory);
        artifacts.manifestWriteStatus = std::string(manifestWriteStatus);
        artifacts.manifestFailureCategory = std::string(manifestFailureCategory);
        artifacts.startupFailureSummaryWriteStatus = std::string(startupFailureSummaryWriteStatus);
        artifacts.startupFailureSummaryFailureCategory = std::string(startupFailureSummaryFailureCategory);
        artifacts.startupFailureManifestWriteStatus = std::string(startupFailureManifestWriteStatus);
        artifacts.startupFailureManifestFailureCategory = std::string(startupFailureManifestFailureCategory);
        artifacts.timestampUtc = std::string(timestampUtc);
        artifacts.gitCommit = std::string(gitCommit);
        artifacts.gitDirty = gitDirty;
        artifacts.buildType = std::string(buildType);
        return artifacts;
    }

    HeadlessStartupFailureArtifactsResult WriteHeadlessStartupFailureArtifacts(const std::filesystem::path& summaryPath,
                                                                               const std::filesystem::path& manifestPath,
                                                                               const std::string_view scenarioKey,
                                                                               const HeadlessRunReportContext& context,
                                                                               const HeadlessRunOutcomeTracker& outcome,
                                                                               const std::string_view failureCategory,
                                                                               const std::string_view timestampUtc,
                                                                               const std::string_view gitCommit,
                                                                               const bool gitDirty,
                                                                               const std::string_view buildType)
    {
        HeadlessStartupFailureArtifactsResult result{};

        HeadlessRunSummary failureSummaryAggregate{};
        failureSummaryAggregate.scenarioKey = std::string(scenarioKey);
        failureSummaryAggregate.frameCount = 0;
        result.summary = BuildHeadlessRunSummaryReport(failureSummaryAggregate, context, outcome);
        result.summary.failureCategory = std::string(failureCategory);

        std::ofstream failureSummaryOut(summaryPath);
        if (failureSummaryOut.is_open())
        {
            result.summaryOpened = true;
            MarkHeadlessArtifactOpened(result.manifest.startupFailureSummaryWriteStatus);
            WriteHeadlessRunSummaryCsvHeader(failureSummaryOut);
            WriteHeadlessRunSummaryCsvRow(failureSummaryOut, result.summary);
            FinalizeHeadlessArtifactWrite(failureSummaryOut,
                                          result.manifest.startupFailureSummaryWriteStatus,
                                          result.manifest.startupFailureSummaryFailureCategory,
                                          "startup_failure_summary_write_failed");
        }

        HeadlessRunArtifactReport failureArtifacts{};
        failureArtifacts.outputPath = "";
        failureArtifacts.metricsPath = "";
        failureArtifacts.summaryPath = summaryPath.empty() ? std::string{} : std::filesystem::absolute(summaryPath).string();
        failureArtifacts.batchIndexPath = "";
        failureArtifacts.batchIndexAppendStatus = "not_requested";
        failureArtifacts.batchIndexFailureCategory = "";
        failureArtifacts.outputWriteStatus = "";
        failureArtifacts.outputFailureCategory = "";
        failureArtifacts.metricsWriteStatus = "";
        failureArtifacts.metricsFailureCategory = "";
        failureArtifacts.summaryWriteStatus = "";
        failureArtifacts.summaryFailureCategory = "";
        failureArtifacts.manifestWriteStatus = "written";
        failureArtifacts.manifestFailureCategory = "";
        failureArtifacts.startupFailureSummaryWriteStatus = result.summaryOpened
            ? result.manifest.startupFailureSummaryWriteStatus
            : "not_applicable";
        failureArtifacts.startupFailureSummaryFailureCategory = result.manifest.startupFailureSummaryFailureCategory;
        failureArtifacts.startupFailureManifestWriteStatus = "pending";
        failureArtifacts.startupFailureManifestFailureCategory = "";
        failureArtifacts.timestampUtc = std::string(timestampUtc);
        failureArtifacts.gitCommit = std::string(gitCommit);
        failureArtifacts.gitDirty = gitDirty;
        failureArtifacts.buildType = std::string(buildType);

        result.manifest = BuildHeadlessRunManifestReport(0u, context, failureArtifacts, outcome);
        result.manifest.failureCategory = std::string(failureCategory);

        std::ofstream failureManifestOut(manifestPath);
        if (failureManifestOut.is_open())
        {
            result.manifestOpened = true;
            MarkHeadlessArtifactOpened(result.manifest.startupFailureManifestWriteStatus);
            WriteHeadlessRunManifestCsvHeader(failureManifestOut);
            WriteHeadlessRunManifestCsvRow(failureManifestOut, result.manifest);
            FinalizeHeadlessArtifactWrite(failureManifestOut,
                                          result.manifest.startupFailureManifestWriteStatus,
                                          result.manifest.startupFailureManifestFailureCategory,
                                          "startup_failure_manifest_write_failed");
        }

        return result;
    }

    HeadlessRunFinalizationResult FinalizeHeadlessRunReports(const std::string_view scenarioKey,
                                                             const HeadlessRunSummaryAccumulator& accumulator,
                                                             const HeadlessRunReportContext& context,
                                                             const HeadlessRunOutcomeTracker& outcome,
                                                             HeadlessRunArtifactReport artifacts,
                                                             std::ostream* summaryStream,
                                                             std::ostream* manifestStream)
    {
        HeadlessRunFinalizationResult result{};
        result.summary = BuildHeadlessRunSummaryReport(accumulator.Build(std::string(scenarioKey)), context, outcome);
        result.artifacts = std::move(artifacts);

        if (summaryStream != nullptr && result.artifacts.summaryFailureCategory.empty())
        {
            WriteHeadlessRunSummaryCsvRow(*summaryStream, result.summary);
            FinalizeHeadlessArtifactWrite(*summaryStream,
                                          result.artifacts.summaryWriteStatus,
                                          result.artifacts.summaryFailureCategory,
                                          "summary_write_failed");
        }

        result.manifest = BuildHeadlessRunManifestReport(result.summary.frameCount,
                                                         context,
                                                         result.artifacts,
                                                         outcome);

        if (!result.artifacts.batchIndexPath.empty())
        {
            AppendHeadlessManifestToBatchIndex(result.artifacts.batchIndexPath,
                                               result.manifest,
                                               result.artifacts.batchIndexAppendStatus,
                                               result.artifacts.batchIndexFailureCategory);
            result.manifest.batchIndexAppendStatus = result.artifacts.batchIndexAppendStatus;
            result.manifest.batchIndexFailureCategory = result.artifacts.batchIndexFailureCategory;
        }

        if (manifestStream != nullptr && result.artifacts.manifestFailureCategory.empty())
        {
            WriteHeadlessRunManifestCsvRow(*manifestStream, result.manifest);
            FinalizeHeadlessArtifactWrite(*manifestStream,
                                          result.artifacts.manifestWriteStatus,
                                          result.artifacts.manifestFailureCategory,
                                          "manifest_write_failed");
        }

        result.manifest.summaryWriteStatus = result.artifacts.summaryWriteStatus;
        result.manifest.summaryFailureCategory = result.artifacts.summaryFailureCategory;
        result.manifest.manifestWriteStatus = result.artifacts.manifestWriteStatus;
        result.manifest.manifestFailureCategory = result.artifacts.manifestFailureCategory;
        return result;
    }

    HeadlessStartupCoordinatorConfig BuildHeadlessStartupCoordinatorConfig(const bool headless,
                                                                           const std::string_view outputPrefix,
                                                                           const std::string_view batchIndexPath,
                                                                           const std::string_view requestedScenarioKey,
                                                                           const std::string_view resolvedScenarioKey,
                                                                           const bool fallbackUsed,
                                                                           const double fixedDtSeconds,
                                                                           const bool boundedFrames,
                                                                           const std::size_t requestedFrames,
                                                                           const std::uint64_t runConfigHash,
                                                                           const std::string_view startupFailureSummaryPath,
                                                                           const std::string_view startupFailureManifestPath,
                                                                           const std::string_view timestampUtc,
                                                                           const std::string_view gitCommit,
                                                                           const bool gitDirty,
                                                                           const std::string_view buildType)
    {
        HeadlessStartupCoordinatorConfig config{};
        config.headless = headless;
        config.outputPrefix = std::string(outputPrefix);
        config.batchIndexPath = std::string(batchIndexPath);
        config.requestedScenarioKey = std::string(requestedScenarioKey);
        config.resolvedScenarioKey = std::string(resolvedScenarioKey);
        config.fallbackUsed = fallbackUsed;
        config.fixedDtSeconds = fixedDtSeconds;
        config.boundedFrames = boundedFrames;
        config.requestedFrames = requestedFrames;
        config.runConfigHash = runConfigHash;
        config.startupFailureSummaryPath = std::string(startupFailureSummaryPath);
        config.startupFailureManifestPath = std::string(startupFailureManifestPath);
        config.timestampUtc = std::string(timestampUtc);
        config.gitCommit = std::string(gitCommit);
        config.gitDirty = gitDirty;
        config.buildType = std::string(buildType);
        return config;
    }

    AppliedHeadlessStartupResult ApplyHeadlessStartupResult(HeadlessStartupCoordinatorResult startup)
    {
        AppliedHeadlessStartupResult applied{};
        applied.outputPath = std::move(startup.bootstrap.outputPath);
        applied.metricsPath = std::move(startup.bootstrap.metricsPath);
        applied.summaryPath = std::move(startup.bootstrap.summaryPath);
        applied.manifestPath = std::move(startup.bootstrap.manifestPath);
        applied.batchIndexAppendStatus = std::move(startup.bootstrap.batchIndexAppendStatus);
        applied.batchIndexFailureCategory = std::move(startup.bootstrap.batchIndexFailureCategory);
        applied.outputWriteStatus = std::move(startup.bootstrap.outputWriteStatus);
        applied.outputFailureCategory = std::move(startup.bootstrap.outputFailureCategory);
        applied.metricsWriteStatus = std::move(startup.bootstrap.metricsWriteStatus);
        applied.metricsFailureCategory = std::move(startup.bootstrap.metricsFailureCategory);
        applied.summaryWriteStatus = std::move(startup.bootstrap.summaryWriteStatus);
        applied.summaryFailureCategory = std::move(startup.bootstrap.summaryFailureCategory);
        applied.manifestWriteStatus = std::move(startup.bootstrap.manifestWriteStatus);
        applied.manifestFailureCategory = std::move(startup.bootstrap.manifestFailureCategory);
        applied.startupFailureSummaryWriteStatus = std::move(startup.startupFailureSummaryWriteStatus);
        applied.startupFailureSummaryFailureCategory = std::move(startup.startupFailureSummaryFailureCategory);
        applied.startupFailureManifestWriteStatus = std::move(startup.startupFailureManifestWriteStatus);
        applied.startupFailureManifestFailureCategory = std::move(startup.startupFailureManifestFailureCategory);
        applied.outputStream = std::move(startup.bootstrap.outputStream);
        applied.metricsStream = std::move(startup.bootstrap.metricsStream);
        applied.summaryStream = std::move(startup.bootstrap.summaryStream);
        applied.manifestStream = std::move(startup.bootstrap.manifestStream);
        return applied;
    }

    HeadlessStartupCoordinatorResult CoordinateHeadlessStartup(ecs::World& world,
                                                               IScenario& scenario,
                                                               HeadlessRunOutcomeTracker& outcome,
                                                               const HeadlessStartupCoordinatorConfig& config,
                                                               const std::function<void(std::string_view)>& maybeFailPhase)
    {
        HeadlessStartupCoordinatorResult result{};

        auto classifyStartupFailure = [&](const std::string_view phase, const std::string& detail = std::string{}) {
            outcome.MarkStartupFailure(phase, detail);
        };

        try
        {
            maybeFailPhase("setup");
            scenario.Setup(world);
        }
        catch (const std::exception& ex)
        {
            classifyStartupFailure("setup", ex.what());
        }
        catch (...)
        {
            classifyStartupFailure("setup");
        }

        if (config.headless)
        {
            const std::string outputBase = config.outputPrefix.empty() ? "headless" : config.outputPrefix;
            result.bootstrap = BootstrapHeadlessArtifacts(std::filesystem::path(outputBase),
                                                          config.batchIndexPath.empty() ? std::filesystem::path{} : std::filesystem::path(config.batchIndexPath));
            if (!result.bootstrap.startupFailureCategory.empty())
            {
                outcome.MarkStartupFailureCategory(result.bootstrap.startupFailureCategory);
            }
        }

        if (outcome.runStatus == "startup_failure")
        {
            HeadlessRunReportContext reportContext{};
            reportContext.requestedScenarioKey = config.requestedScenarioKey;
            reportContext.resolvedScenarioKey = config.resolvedScenarioKey;
            reportContext.fallbackUsed = config.fallbackUsed;
            reportContext.fixedDtSeconds = config.fixedDtSeconds;
            reportContext.boundedFrames = config.boundedFrames;
            reportContext.requestedFrames = config.requestedFrames;
            reportContext.headless = config.headless;
            reportContext.runConfigHash = config.runConfigHash;
            reportContext.terminationReason = outcome.DeriveTerminationReason(config.boundedFrames,
                                                                              config.headless,
                                                                              false,
                                                                              false);

            const auto startupFailureArtifacts = WriteHeadlessStartupFailureArtifacts(config.startupFailureSummaryPath,
                                                                                      config.startupFailureManifestPath,
                                                                                      config.resolvedScenarioKey,
                                                                                      reportContext,
                                                                                      outcome,
                                                                                      outcome.failureCategory,
                                                                                      config.timestampUtc,
                                                                                      config.gitCommit,
                                                                                      config.gitDirty,
                                                                                      config.buildType);
            result.startupFailureSummaryWriteStatus = startupFailureArtifacts.manifest.startupFailureSummaryWriteStatus;
            result.startupFailureSummaryFailureCategory = startupFailureArtifacts.manifest.startupFailureSummaryFailureCategory;
            result.startupFailureManifestWriteStatus = startupFailureArtifacts.manifest.startupFailureManifestWriteStatus;
            result.startupFailureManifestFailureCategory = startupFailureArtifacts.manifest.startupFailureManifestFailureCategory;
            result.startupFailureSummaryOpened = startupFailureArtifacts.summaryOpened;
            result.startupFailureManifestOpened = startupFailureArtifacts.manifestOpened;
        }

        return result;
    }

    void LogHeadlessStartupMessages(const core::Logger& logger,
                                    const HeadlessStartupCoordinatorResult& startup,
                                    const HeadlessRunOutcomeTracker& outcome,
                                    const std::string_view outputPath,
                                    const std::string_view metricsPath,
                                    const std::string_view summaryPath,
                                    const std::string_view manifestPath,
                                    const std::string_view batchIndexPath,
                                    const std::string_view startupFailureSummaryPath,
                                    const std::string_view startupFailureManifestPath)
    {
        if (!startup.bootstrap.startupFailureCategory.empty())
        {
            if (startup.bootstrap.startupFailureCategory == "output_directory_create_failed")
            {
                logger.Error(std::string("Failed to create output directory: ")
                             + std::filesystem::path(outputPath).parent_path().string());
            }
            else if (startup.bootstrap.startupFailureCategory == "output_file_open_failed")
            {
                logger.Error(std::string("Failed to open ") + std::string(outputPath));
            }
            else if (startup.bootstrap.startupFailureCategory == "metrics_file_open_failed")
            {
                logger.Error(std::string("Failed to open ") + std::string(metricsPath));
            }
            else if (startup.bootstrap.startupFailureCategory == "summary_file_open_failed")
            {
                logger.Error(std::string("Failed to open ") + std::string(summaryPath));
            }
            else if (startup.bootstrap.startupFailureCategory == "manifest_file_open_failed")
            {
                logger.Error(std::string("Failed to open ") + std::string(manifestPath));
            }
        }

        if (startup.bootstrap.batchIndexFailureCategory == "batch_index_open_failed" && !batchIndexPath.empty())
        {
            logger.Error(std::string("Failed to create batch index directory: ")
                         + std::filesystem::path(batchIndexPath).parent_path().string());
        }

        auto logHeadlessArtifactPath = [&](const std::string& label, const std::string_view path) {
            try
            {
                const auto cwd = std::filesystem::current_path();
                logger.Info(std::string("Headless ") + label + " path: " + (cwd / std::filesystem::path(path)).string());
            }
            catch (...)
            {
                logger.Warn(std::string("Could not determine current working directory for headless ") + label);
            }
        };

        if (startup.bootstrap.outputStream.is_open())
        {
            logHeadlessArtifactPath("output", outputPath);
        }
        if (startup.bootstrap.metricsStream.is_open())
        {
            logHeadlessArtifactPath("metrics", metricsPath);
        }
        if (startup.bootstrap.summaryStream.is_open())
        {
            logHeadlessArtifactPath("summary", summaryPath);
        }
        if (startup.bootstrap.manifestStream.is_open())
        {
            logHeadlessArtifactPath("manifest", manifestPath);
        }

        if (outcome.runStatus == "startup_failure")
        {
            if (!startup.startupFailureSummaryOpened)
            {
                logger.Error(std::string("Failed to open startup failure summary: ") + std::string(startupFailureSummaryPath));
            }
            if (!startup.startupFailureManifestOpened)
            {
                logger.Error(std::string("Failed to open startup failure manifest: ") + std::string(startupFailureManifestPath));
            }
        }
    }

    void LogHeadlessFinalizationMessages(const core::Logger& logger,
                                         const std::string_view batchIndexPath,
                                         const std::string_view batchIndexFailureCategory)
    {
        if (batchIndexFailureCategory == "batch_index_open_failed" && !batchIndexPath.empty())
        {
            logger.Error(std::string("Failed to open batch index: ") + std::string(batchIndexPath));
        }
    }

    HeadlessRunFinalizationPreparation PrepareHeadlessRunFinalization(const HeadlessRunOutcomeTracker& outcome,
                                                                      const std::string_view requestedScenarioKey,
                                                                      const std::string_view resolvedScenarioKey,
                                                                      const bool fallbackUsed,
                                                                      const double fixedDtSeconds,
                                                                      const bool boundedFrames,
                                                                      const std::size_t requestedFrames,
                                                                      const bool headless,
                                                                      const std::uint64_t runConfigHash,
                                                                      const bool quitRequestedByInput,
                                                                      const bool quitRequestedByEof,
                                                                      const std::string_view outputPath,
                                                                      const std::string_view metricsPath,
                                                                      const std::string_view summaryPath,
                                                                      const std::string_view batchIndexPath,
                                                                      const std::string_view batchIndexAppendStatus,
                                                                      const std::string_view batchIndexFailureCategory,
                                                                      const std::string_view outputWriteStatus,
                                                                      const std::string_view outputFailureCategory,
                                                                      const std::string_view metricsWriteStatus,
                                                                      const std::string_view metricsFailureCategory,
                                                                      const std::string_view summaryWriteStatus,
                                                                      const std::string_view summaryFailureCategory,
                                                                      const std::string_view manifestWriteStatus,
                                                                      const std::string_view manifestFailureCategory,
                                                                      const std::string_view startupFailureSummaryWriteStatus,
                                                                      const std::string_view startupFailureSummaryFailureCategory,
                                                                      const std::string_view startupFailureManifestWriteStatus,
                                                                      const std::string_view startupFailureManifestFailureCategory,
                                                                      const std::string_view timestampUtc,
                                                                      const std::string_view gitCommit,
                                                                      const bool gitDirty,
                                                                      const std::string_view buildType)
    {
        HeadlessRunFinalizationPreparation result{};
        result.context.requestedScenarioKey = std::string(requestedScenarioKey);
        result.context.resolvedScenarioKey = std::string(resolvedScenarioKey);
        result.context.fallbackUsed = fallbackUsed;
        result.context.fixedDtSeconds = fixedDtSeconds;
        result.context.boundedFrames = boundedFrames;
        result.context.requestedFrames = requestedFrames;
        result.context.headless = headless;
        result.context.runConfigHash = runConfigHash;
        result.context.terminationReason = outcome.DeriveTerminationReason(boundedFrames,
                                                                           headless,
                                                                           quitRequestedByInput,
                                                                           quitRequestedByEof);

        result.artifacts = BuildNormalHeadlessArtifactReport(HeadlessRunArtifactReport{},
                                                             outputPath,
                                                             metricsPath,
                                                             summaryPath,
                                                             batchIndexPath,
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
                                                             timestampUtc,
                                                             gitCommit,
                                                             gitDirty,
                                                             buildType);
        return result;
    }

    void HeadlessRunSummaryAccumulator::AddFrame(const FrameMetrics& metrics)
    {
        ++m_frameCount;
        m_finalWorldHash = metrics.worldHash;
        m_totalCollisionCount += metrics.collisionCount;
        m_peakCollisionCount = std::max(m_peakCollisionCount, metrics.collisionCount);
        m_maxRigidBodyCount = std::max(m_maxRigidBodyCount, metrics.rigidBodyCount);
        m_maxDynamicBodyCount = std::max(m_maxDynamicBodyCount, metrics.dynamicBodyCount);
        m_maxTransformCount = std::max(m_maxTransformCount, metrics.transformCount);

        m_totalUpdateWallSeconds += metrics.updateWallSeconds;
        m_totalRenderWallSeconds += metrics.renderWallSeconds;
        m_totalFrameWallSeconds += metrics.frameWallSeconds;
        m_updateWallSamples.push_back(metrics.updateWallSeconds);
        m_renderWallSamples.push_back(metrics.renderWallSeconds);
        m_frameWallSamples.push_back(metrics.frameWallSeconds);
    }

    HeadlessRunSummary HeadlessRunSummaryAccumulator::Build(const std::string& scenarioKey) const
    {
        HeadlessRunSummary summary{};
        summary.scenarioKey = scenarioKey;
        summary.requestedScenarioKey = scenarioKey;
        summary.resolvedScenarioKey = scenarioKey;
        summary.frameCount = m_frameCount;
        summary.finalWorldHash = m_finalWorldHash;
        summary.totalCollisionCount = m_totalCollisionCount;
        summary.peakCollisionCount = m_peakCollisionCount;
        summary.maxRigidBodyCount = m_maxRigidBodyCount;
        summary.maxDynamicBodyCount = m_maxDynamicBodyCount;
        summary.maxTransformCount = m_maxTransformCount;
        summary.avgUpdateWallSeconds = AverageOrZero(m_totalUpdateWallSeconds, m_frameCount);
        summary.p95UpdateWallSeconds = NearestRankPercentile(m_updateWallSamples, 95.0);
        summary.avgRenderWallSeconds = AverageOrZero(m_totalRenderWallSeconds, m_frameCount);
        summary.p95RenderWallSeconds = NearestRankPercentile(m_renderWallSamples, 95.0);
        summary.avgFrameWallSeconds = AverageOrZero(m_totalFrameWallSeconds, m_frameCount);
        summary.p95FrameWallSeconds = NearestRankPercentile(m_frameWallSamples, 95.0);
        return summary;
    }

    FrameMetrics CaptureFrameMetrics(const ecs::World& world,
                                     const physics::PhysicsSystem& physicsSystem,
                                     std::size_t frameIndex,
                                     double simTimeSeconds) noexcept
    {
        FrameMetrics metrics{};
        metrics.frameIndex = frameIndex;
        metrics.simTimeSeconds = simTimeSeconds;

        WorldHasher hasher;
        metrics.worldHash = hasher.HashWorld(world);
        metrics.collisionCount = physicsSystem.GetCollisionEvents().size();

        if (const auto* rigidBodies = world.GetStorage<physics::RigidBodyComponent>())
        {
            metrics.rigidBodyCount = rigidBodies->Size();
            rigidBodies->ForEach([&](ecs::EntityId, const physics::RigidBodyComponent& body) {
                if (body.invMass > 0.0f)
                {
                    ++metrics.dynamicBodyCount;
                }
            });
        }

        if (const auto* transforms = world.GetStorage<physics::TransformComponent>())
        {
            metrics.transformCount = transforms->Size();
        }

        return metrics;
    }

    std::string_view ClassifyHeadlessFailurePhase(const std::string_view phase, const bool startupPhase) noexcept
    {
        if (startupPhase)
        {
            if (phase == "setup")
            {
                return "scenario_setup_failed";
            }
            return phase;
        }

        if (phase == "update")
        {
            return "scenario_update_failed";
        }
        if (phase == "world_update")
        {
            return "world_update_failed";
        }
        if (phase == "render")
        {
            return "scenario_render_failed";
        }
        return "runtime_exception";
    }

    std::uint64_t HashHeadlessRunConfig(const std::string& scenarioKey,
                                        const double fixedDtSeconds,
                                        const std::size_t requestedFrames,
                                        const bool headless) noexcept
    {
        std::uint64_t hash = kFnvOffsetBasis;
        HashString(hash, scenarioKey);
        HashValue(hash, fixedDtSeconds);
        HashValue(hash, requestedFrames);
        const std::uint8_t headlessByte = headless ? 1u : 0u;
        HashValue(hash, headlessByte);
        return hash;
    }

    void WriteFrameMetricsCsvHeader(std::ostream& out)
    {
        out << "frame,sim_time_seconds,world_hash,collision_count,rigid_body_count,dynamic_body_count,transform_count,update_wall_seconds,render_wall_seconds,frame_wall_seconds\n";
    }

    void WriteFrameMetricsCsvRow(std::ostream& out, const FrameMetrics& metrics)
    {
        const auto previousFlags = out.flags();
        const auto previousPrecision = out.precision();

        out << metrics.frameIndex << ','
            << std::fixed << std::setprecision(6) << metrics.simTimeSeconds << ','
            << metrics.worldHash << ','
            << metrics.collisionCount << ','
            << metrics.rigidBodyCount << ','
            << metrics.dynamicBodyCount << ','
            << metrics.transformCount << ','
            << metrics.updateWallSeconds << ','
            << metrics.renderWallSeconds << ','
            << metrics.frameWallSeconds << '\n';

        out.flags(previousFlags);
        out.precision(previousPrecision);
    }

    void WriteHeadlessRunSummaryCsvHeader(std::ostream& out)
    {
        out << "requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,bounded_frames,requested_frames,headless,run_config_hash,frame_count,run_status,failure_category,failure_detail,termination_reason,final_world_hash,total_collision_count,peak_collision_count,max_rigid_body_count,max_dynamic_body_count,max_transform_count,avg_update_wall_seconds,p95_update_wall_seconds,avg_render_wall_seconds,p95_render_wall_seconds,avg_frame_wall_seconds,p95_frame_wall_seconds\n";
    }

    void WriteHeadlessRunSummaryCsvRow(std::ostream& out, const HeadlessRunSummary& summary)
    {
        const auto previousFlags = out.flags();
        const auto previousPrecision = out.precision();

        out << summary.requestedScenarioKey << ','
            << summary.resolvedScenarioKey << ','
            << (summary.fallbackUsed ? 1 : 0) << ','
            << std::fixed << std::setprecision(6) << summary.fixedDtSeconds << ','
            << (summary.boundedFrames ? 1 : 0) << ','
            << summary.requestedFrames << ','
            << (summary.headless ? 1 : 0) << ','
            << summary.runConfigHash << ','
            << summary.frameCount << ','
            << summary.runStatus << ','
            << summary.failureCategory << ','
            << summary.failureDetail << ','
            << summary.terminationReason << ','
            << summary.finalWorldHash << ','
            << summary.totalCollisionCount << ','
            << summary.peakCollisionCount << ','
            << summary.maxRigidBodyCount << ','
            << summary.maxDynamicBodyCount << ','
            << summary.maxTransformCount << ','
            << summary.avgUpdateWallSeconds << ','
            << summary.p95UpdateWallSeconds << ','
            << summary.avgRenderWallSeconds << ','
            << summary.p95RenderWallSeconds << ','
            << summary.avgFrameWallSeconds << ','
            << summary.p95FrameWallSeconds << '\n';

        out.flags(previousFlags);
        out.precision(previousPrecision);
    }

    void WriteHeadlessRunManifestCsvHeader(std::ostream& out)
    {
        out << "requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,bounded_frames,requested_frames,headless,run_config_hash,frame_count,run_status,failure_category,failure_detail,termination_reason,output_path,metrics_path,summary_path,batch_index_path,batch_index_append_status,batch_index_failure_category,output_write_status,output_failure_category,metrics_write_status,metrics_failure_category,summary_write_status,summary_failure_category,manifest_write_status,manifest_failure_category,startup_failure_summary_write_status,startup_failure_summary_failure_category,startup_failure_manifest_write_status,startup_failure_manifest_failure_category,exit_code,exit_classification,timestamp_utc,git_commit,git_dirty,build_type\n";
    }

    void WriteHeadlessRunManifestCsvRow(std::ostream& out, const HeadlessRunManifest& manifest)
    {
        const auto previousFlags = out.flags();
        const auto previousPrecision = out.precision();

        out << manifest.requestedScenarioKey << ','
            << manifest.resolvedScenarioKey << ','
            << (manifest.fallbackUsed ? 1 : 0) << ','
            << std::fixed << std::setprecision(6) << manifest.fixedDtSeconds << ','
            << (manifest.boundedFrames ? 1 : 0) << ','
            << manifest.requestedFrames << ','
            << (manifest.headless ? 1 : 0) << ','
            << manifest.runConfigHash << ','
            << manifest.frameCount << ','
            << manifest.runStatus << ','
            << manifest.failureCategory << ','
            << manifest.failureDetail << ','
            << manifest.terminationReason << ','
            << manifest.outputPath << ','
            << manifest.metricsPath << ','
            << manifest.summaryPath << ','
            << manifest.batchIndexPath << ','
            << manifest.batchIndexAppendStatus << ','
            << manifest.batchIndexFailureCategory << ','
            << manifest.outputWriteStatus << ','
            << manifest.outputFailureCategory << ','
            << manifest.metricsWriteStatus << ','
            << manifest.metricsFailureCategory << ','
            << manifest.summaryWriteStatus << ','
            << manifest.summaryFailureCategory << ','
            << manifest.manifestWriteStatus << ','
            << manifest.manifestFailureCategory << ','
            << manifest.startupFailureSummaryWriteStatus << ','
            << manifest.startupFailureSummaryFailureCategory << ','
            << manifest.startupFailureManifestWriteStatus << ','
            << manifest.startupFailureManifestFailureCategory << ','
            << manifest.exitCode << ','
            << manifest.exitClassification << ','
            << manifest.timestampUtc << ','
            << manifest.gitCommit << ','
            << (manifest.gitDirty ? 1 : 0) << ','
            << manifest.buildType << '\n';

        out.flags(previousFlags);
        out.precision(previousPrecision);
    }
}
