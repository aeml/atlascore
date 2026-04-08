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
#include <iomanip>
#include <ostream>
#include <vector>

namespace
{
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
}

namespace simlab
{
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
        out << "scenario_key,frame_count,final_world_hash,total_collision_count,peak_collision_count,max_rigid_body_count,max_dynamic_body_count,max_transform_count,avg_update_wall_seconds,p95_update_wall_seconds,avg_render_wall_seconds,p95_render_wall_seconds,avg_frame_wall_seconds,p95_frame_wall_seconds\n";
    }

    void WriteHeadlessRunSummaryCsvRow(std::ostream& out, const HeadlessRunSummary& summary)
    {
        const auto previousFlags = out.flags();
        const auto previousPrecision = out.precision();

        out << summary.scenarioKey << ','
            << summary.frameCount << ','
            << summary.finalWorldHash << ','
            << summary.totalCollisionCount << ','
            << summary.peakCollisionCount << ','
            << summary.maxRigidBodyCount << ','
            << summary.maxDynamicBodyCount << ','
            << summary.maxTransformCount << ','
            << std::fixed << std::setprecision(6) << summary.avgUpdateWallSeconds << ','
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
        out << "scenario_key,frame_count,output_path,metrics_path,summary_path,timestamp_utc\n";
    }

    void WriteHeadlessRunManifestCsvRow(std::ostream& out, const HeadlessRunManifest& manifest)
    {
        out << manifest.scenarioKey << ','
            << manifest.frameCount << ','
            << manifest.outputPath << ','
            << manifest.metricsPath << ','
            << manifest.summaryPath << ','
            << manifest.timestampUtc << '\n';
    }
}
