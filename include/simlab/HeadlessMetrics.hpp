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
#include <iosfwd>
#include <string>
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
        std::string timestampUtc;
        std::string gitCommit;
        bool gitDirty{false};
        std::string buildType;
    };

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
