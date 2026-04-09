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
#include "simlab/WorldHasher.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <ostream>
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
