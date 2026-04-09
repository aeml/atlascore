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

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace ecs { class World; }
namespace physics { class PhysicsSystem; }

namespace simlab
{
    struct FrameMetrics
    {
        std::size_t frameIndex{0};
        double simTimeSeconds{0.0};
        std::uint64_t worldHash{0};
        std::size_t collisionCount{0};
        std::size_t rigidBodyCount{0};
        std::size_t dynamicBodyCount{0};
        std::size_t transformCount{0};
        double updateWallSeconds{0.0};
        double renderWallSeconds{0.0};
        double frameWallSeconds{0.0};
    };

    struct HeadlessRunSummary
    {
        std::string scenarioKey;
        std::string requestedScenarioKey;
        std::string resolvedScenarioKey;
        bool fallbackUsed{false};
        double fixedDtSeconds{0.0};
        bool boundedFrames{false};
        std::size_t requestedFrames{0};
        bool headless{false};
        std::uint64_t runConfigHash{0};
        std::size_t frameCount{0};
        std::string runStatus;
        std::string failureCategory;
        std::string failureDetail;
        std::string terminationReason;
        std::uint64_t finalWorldHash{0};
        std::size_t totalCollisionCount{0};
        std::size_t peakCollisionCount{0};
        std::size_t maxRigidBodyCount{0};
        std::size_t maxDynamicBodyCount{0};
        std::size_t maxTransformCount{0};
        double avgUpdateWallSeconds{0.0};
        double p95UpdateWallSeconds{0.0};
        double avgRenderWallSeconds{0.0};
        double p95RenderWallSeconds{0.0};
        double avgFrameWallSeconds{0.0};
        double p95FrameWallSeconds{0.0};
    };

    struct HeadlessRunManifest
    {
        std::string scenarioKey;
        std::string requestedScenarioKey;
        std::string resolvedScenarioKey;
        bool fallbackUsed{false};
        double fixedDtSeconds{0.0};
        bool boundedFrames{false};
        std::size_t requestedFrames{0};
        bool headless{false};
        std::uint64_t runConfigHash{0};
        std::size_t frameCount{0};
        std::string runStatus;
        std::string failureCategory;
        std::string failureDetail;
        std::string terminationReason;
        std::string outputPath;
        std::string metricsPath;
        std::string summaryPath;
        std::string batchIndexPath;
        std::string batchIndexAppendStatus;
        std::string batchIndexFailureCategory;
        std::string outputWriteStatus;
        std::string outputFailureCategory;
        std::string metricsWriteStatus;
        std::string metricsFailureCategory;
        std::string summaryWriteStatus;
        std::string summaryFailureCategory;
        std::string manifestWriteStatus;
        std::string manifestFailureCategory;
        std::string startupFailureSummaryWriteStatus;
        std::string startupFailureSummaryFailureCategory;
        std::string startupFailureManifestWriteStatus;
        std::string startupFailureManifestFailureCategory;
        int exitCode{0};
        std::string exitClassification;
        std::string timestampUtc;
        std::string gitCommit;
        bool gitDirty{false};
        std::string buildType;
    };

    struct HeadlessRunOutcomeTracker
    {
        std::string runStatus{"success"};
        std::string failureCategory;
        std::string failureDetail;
        int exitCode{0};
        std::string exitClassification{"success_exit"};

        void MarkStartupFailure(std::string_view phase, std::string_view detail = {}) noexcept;
        void MarkStartupFailureCategory(std::string_view category, std::string_view detail = {}) noexcept;
        void MarkRuntimeFailure(std::string_view phase, std::string_view detail = {}) noexcept;
        std::string DeriveTerminationReason(bool boundedFrames,
                                            bool headless,
                                            bool quitRequestedByInput,
                                            bool quitRequestedByEof) const;
    };

    struct HeadlessRunReportContext
    {
        std::string requestedScenarioKey;
        std::string resolvedScenarioKey;
        bool fallbackUsed{false};
        double fixedDtSeconds{0.0};
        bool boundedFrames{false};
        std::size_t requestedFrames{0};
        bool headless{false};
        std::uint64_t runConfigHash{0};
        std::string terminationReason;
    };

    struct HeadlessRunArtifactReport
    {
        std::string outputPath;
        std::string metricsPath;
        std::string summaryPath;
        std::string batchIndexPath;
        std::string batchIndexAppendStatus;
        std::string batchIndexFailureCategory;
        std::string outputWriteStatus;
        std::string outputFailureCategory;
        std::string metricsWriteStatus;
        std::string metricsFailureCategory;
        std::string summaryWriteStatus;
        std::string summaryFailureCategory;
        std::string manifestWriteStatus;
        std::string manifestFailureCategory;
        std::string startupFailureSummaryWriteStatus;
        std::string startupFailureSummaryFailureCategory;
        std::string startupFailureManifestWriteStatus;
        std::string startupFailureManifestFailureCategory;
        std::string timestampUtc;
        std::string gitCommit;
        bool gitDirty{false};
        std::string buildType;
    };

    struct HeadlessStartupFailureArtifactsResult
    {
        HeadlessRunSummary summary;
        HeadlessRunManifest manifest;
        bool summaryOpened{false};
        bool manifestOpened{false};
    };

    HeadlessRunSummary BuildHeadlessRunSummaryReport(const HeadlessRunSummary& aggregate,
                                                     const HeadlessRunReportContext& context,
                                                     const HeadlessRunOutcomeTracker& outcome);
    HeadlessRunManifest BuildHeadlessRunManifestReport(std::size_t frameCount,
                                                       const HeadlessRunReportContext& context,
                                                       const HeadlessRunArtifactReport& artifacts,
                                                       const HeadlessRunOutcomeTracker& outcome);
    void MarkHeadlessArtifactOpened(std::string& writeStatus);
    void FinalizeHeadlessArtifactWrite(std::ostream& out,
                                       std::string& writeStatus,
                                       std::string& failureCategory,
                                       std::string_view writeFailureCategory);
    void FinalizeHeadlessBatchAppend(std::ostream& out,
                                     std::string& appendStatus,
                                     std::string& failureCategory,
                                     std::string_view writeFailureCategory);
    void AppendHeadlessManifestToBatchIndex(const std::filesystem::path& batchIndexPath,
                                            const HeadlessRunManifest& manifest,
                                            std::string& appendStatus,
                                            std::string& failureCategory);
    HeadlessRunArtifactReport BuildNormalHeadlessArtifactReport(const HeadlessRunArtifactReport& base,
                                                                std::string_view outputPath,
                                                                std::string_view metricsPath,
                                                                std::string_view summaryPath,
                                                                std::string_view batchIndexPath,
                                                                std::string_view batchIndexAppendStatus,
                                                                std::string_view batchIndexFailureCategory,
                                                                std::string_view outputWriteStatus,
                                                                std::string_view outputFailureCategory,
                                                                std::string_view metricsWriteStatus,
                                                                std::string_view metricsFailureCategory,
                                                                std::string_view summaryWriteStatus,
                                                                std::string_view summaryFailureCategory,
                                                                std::string_view manifestWriteStatus,
                                                                std::string_view manifestFailureCategory,
                                                                std::string_view startupFailureSummaryWriteStatus,
                                                                std::string_view startupFailureSummaryFailureCategory,
                                                                std::string_view startupFailureManifestWriteStatus,
                                                                std::string_view startupFailureManifestFailureCategory,
                                                                std::string_view timestampUtc,
                                                                std::string_view gitCommit,
                                                                bool gitDirty,
                                                                std::string_view buildType);
    HeadlessStartupFailureArtifactsResult WriteHeadlessStartupFailureArtifacts(const std::filesystem::path& summaryPath,
                                                                               const std::filesystem::path& manifestPath,
                                                                               std::string_view scenarioKey,
                                                                               const HeadlessRunReportContext& context,
                                                                               const HeadlessRunOutcomeTracker& outcome,
                                                                               std::string_view failureCategory,
                                                                               std::string_view timestampUtc,
                                                                               std::string_view gitCommit,
                                                                               bool gitDirty,
                                                                               std::string_view buildType);

    class HeadlessRunSummaryAccumulator
    {
    public:
        void AddFrame(const FrameMetrics& metrics);
        HeadlessRunSummary Build(const std::string& scenarioKey) const;

    private:
        std::size_t m_frameCount{0};
        std::uint64_t m_finalWorldHash{0};
        std::size_t m_totalCollisionCount{0};
        std::size_t m_peakCollisionCount{0};
        std::size_t m_maxRigidBodyCount{0};
        std::size_t m_maxDynamicBodyCount{0};
        std::size_t m_maxTransformCount{0};
        double m_totalUpdateWallSeconds{0.0};
        double m_totalRenderWallSeconds{0.0};
        double m_totalFrameWallSeconds{0.0};
        std::vector<double> m_updateWallSamples;
        std::vector<double> m_renderWallSamples;
        std::vector<double> m_frameWallSamples;
    };

    FrameMetrics CaptureFrameMetrics(const ecs::World& world,
                                     const physics::PhysicsSystem& physicsSystem,
                                     std::size_t frameIndex,
                                     double simTimeSeconds) noexcept;

    std::string_view ClassifyHeadlessFailurePhase(std::string_view phase, bool startupPhase) noexcept;

    std::uint64_t HashHeadlessRunConfig(const std::string& scenarioKey,
                                        double fixedDtSeconds,
                                        std::size_t requestedFrames,
                                        bool headless) noexcept;

    void WriteFrameMetricsCsvHeader(std::ostream& out);
    void WriteFrameMetricsCsvRow(std::ostream& out, const FrameMetrics& metrics);
    void WriteHeadlessRunSummaryCsvHeader(std::ostream& out);
    void WriteHeadlessRunSummaryCsvRow(std::ostream& out, const HeadlessRunSummary& summary);
    void WriteHeadlessRunManifestCsvHeader(std::ostream& out);
    void WriteHeadlessRunManifestCsvRow(std::ostream& out, const HeadlessRunManifest& manifest);
}
