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

#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "simlab/HeadlessMetrics.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    void VerifyFrameMetricsCapturePhysicsState()
    {
        ecs::World world;
        auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
        auto* physicsPtr = physicsSystem.get();
        physics::PhysicsSettings settings;
        settings.substeps = 1;
        settings.positionIterations = 1;
        settings.velocityIterations = 1;
        physicsSystem->SetSettings(settings);
        world.AddSystem(std::move(physicsSystem));

        auto a = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(a, physics::TransformComponent{0.0f, 0.0f, 0.0f});
        auto& rbA = world.AddComponent<physics::RigidBodyComponent>(a);
        rbA.mass = 1.0f;
        rbA.invMass = 1.0f;
        world.AddComponent<physics::AABBComponent>(a, physics::AABBComponent{-0.5f, -0.5f, 0.5f, 0.5f});

        auto b = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(b, physics::TransformComponent{0.25f, 0.0f, 0.0f});
        auto& rbB = world.AddComponent<physics::RigidBodyComponent>(b);
        rbB.mass = 1.0f;
        rbB.invMass = 1.0f;
        world.AddComponent<physics::AABBComponent>(b, physics::AABBComponent{-0.25f, -0.5f, 0.75f, 0.5f});

        world.Update(1.0f / 60.0f);

        const auto metrics = simlab::CaptureFrameMetrics(world, *physicsPtr, 3, 0.05);
        assert(metrics.frameIndex == 3u);
        assert(metrics.simTimeSeconds == 0.05);
        assert(metrics.worldHash != 0);
        assert(metrics.collisionCount >= 1u);
        assert(metrics.rigidBodyCount == 2u);
        assert(metrics.dynamicBodyCount == 2u);
        assert(metrics.transformCount == 2u);
        assert(metrics.updateWallSeconds == 0.0);
        assert(metrics.renderWallSeconds == 0.0);
        assert(metrics.frameWallSeconds == 0.0);
    }

    void VerifyCsvWriterProducesStableHeaderAndRow()
    {
        simlab::FrameMetrics metrics{};
        metrics.frameIndex = 7;
        metrics.simTimeSeconds = 0.125;
        metrics.worldHash = 42;
        metrics.collisionCount = 3;
        metrics.rigidBodyCount = 5;
        metrics.dynamicBodyCount = 4;
        metrics.transformCount = 6;
        metrics.updateWallSeconds = 0.001234;
        metrics.renderWallSeconds = 0.000321;
        metrics.frameWallSeconds = 0.001555;

        std::ostringstream out;
        simlab::WriteFrameMetricsCsvHeader(out);
        simlab::WriteFrameMetricsCsvRow(out, metrics);

        const std::string csv = out.str();
        assert(csv.find("frame,sim_time_seconds,world_hash,collision_count,rigid_body_count,dynamic_body_count,transform_count,update_wall_seconds,render_wall_seconds,frame_wall_seconds\n") == 0);
        assert(csv.find("7,0.125000,42,3,5,4,6,0.001234,0.000321,0.001555\n") != std::string::npos);
    }

    void VerifySummaryAccumulatorTracksFinalHashAndAggregates()
    {
        simlab::HeadlessRunSummary summary{};
        summary.scenarioKey = "gravity";

        simlab::HeadlessRunSummaryAccumulator accumulator;
        accumulator.AddFrame(simlab::FrameMetrics{.frameIndex = 1,
                                                  .simTimeSeconds = 1.0 / 60.0,
                                                  .worldHash = 11,
                                                  .collisionCount = 2,
                                                  .rigidBodyCount = 4,
                                                  .dynamicBodyCount = 3,
                                                  .transformCount = 4,
                                                  .updateWallSeconds = 0.001000,
                                                  .renderWallSeconds = 0.000400,
                                                  .frameWallSeconds = 0.001500});
        accumulator.AddFrame(simlab::FrameMetrics{.frameIndex = 2,
                                                  .simTimeSeconds = 2.0 / 60.0,
                                                  .worldHash = 22,
                                                  .collisionCount = 5,
                                                  .rigidBodyCount = 6,
                                                  .dynamicBodyCount = 4,
                                                  .transformCount = 6,
                                                  .updateWallSeconds = 0.003000,
                                                  .renderWallSeconds = 0.000700,
                                                  .frameWallSeconds = 0.003900});
        accumulator.AddFrame(simlab::FrameMetrics{.frameIndex = 3,
                                                  .simTimeSeconds = 3.0 / 60.0,
                                                  .worldHash = 33,
                                                  .collisionCount = 1,
                                                  .rigidBodyCount = 5,
                                                  .dynamicBodyCount = 4,
                                                  .transformCount = 5,
                                                  .updateWallSeconds = 0.002000,
                                                  .renderWallSeconds = 0.000500,
                                                  .frameWallSeconds = 0.002700});

        summary = accumulator.Build("gravity");
        assert(summary.scenarioKey == "gravity");
        assert(summary.frameCount == 3u);
        assert(summary.finalWorldHash == 33u);
        assert(summary.totalCollisionCount == 8u);
        assert(summary.peakCollisionCount == 5u);
        assert(summary.maxRigidBodyCount == 6u);
        assert(summary.maxDynamicBodyCount == 4u);
        assert(summary.maxTransformCount == 6u);
        assert(summary.avgUpdateWallSeconds == 0.002000);
        assert(summary.avgRenderWallSeconds == 0.0005333333333333334);
        assert(summary.avgFrameWallSeconds == 0.0026999999999999997);
    }

    void VerifySummaryAccumulatorUsesTrueNearestRankPercentiles()
    {
        simlab::HeadlessRunSummaryAccumulator accumulator;
        for (std::size_t i = 0; i < 20; ++i)
        {
            const double update = 0.001 * static_cast<double>(i + 1);
            const double render = 0.0001 * static_cast<double>(i + 1);
            const double frame = update + render;
            accumulator.AddFrame(simlab::FrameMetrics{.frameIndex = i + 1,
                                                      .simTimeSeconds = static_cast<double>(i + 1) / 60.0,
                                                      .worldHash = static_cast<std::uint64_t>(100 + i),
                                                      .collisionCount = i,
                                                      .rigidBodyCount = 10 + i,
                                                      .dynamicBodyCount = 9 + i,
                                                      .transformCount = 11 + i,
                                                      .updateWallSeconds = update,
                                                      .renderWallSeconds = render,
                                                      .frameWallSeconds = frame});
        }

        const auto summary = accumulator.Build("percentile");
        assert(summary.frameCount == 20u);
        assert(summary.p95UpdateWallSeconds == 0.019000);
        assert(summary.p95RenderWallSeconds == 0.001900);
        assert(summary.p95FrameWallSeconds == 0.020900);
    }

    void VerifySummaryCsvWriterProducesStableHeaderAndRow()
    {
        simlab::HeadlessRunSummary summary{};
        summary.requestedScenarioKey = "fluid";
        summary.resolvedScenarioKey = "fluid";
        summary.fallbackUsed = false;
        summary.fixedDtSeconds = 1.0 / 120.0;
        summary.boundedFrames = true;
        summary.requestedFrames = 300;
        summary.headless = true;
        summary.runConfigHash = 777;
        summary.frameCount = 300;
        summary.runStatus = "success";
        summary.failureCategory = "";
        summary.failureDetail = "";
        summary.terminationReason = "frame_cap";
        summary.finalWorldHash = 123456;
        summary.totalCollisionCount = 900;
        summary.peakCollisionCount = 17;
        summary.maxRigidBodyCount = 250;
        summary.maxDynamicBodyCount = 240;
        summary.maxTransformCount = 260;
        summary.avgUpdateWallSeconds = 0.010000;
        summary.p95UpdateWallSeconds = 0.020000;
        summary.avgRenderWallSeconds = 0.003000;
        summary.p95RenderWallSeconds = 0.004000;
        summary.avgFrameWallSeconds = 0.013500;
        summary.p95FrameWallSeconds = 0.024500;

        std::ostringstream out;
        simlab::WriteHeadlessRunSummaryCsvHeader(out);
        simlab::WriteHeadlessRunSummaryCsvRow(out, summary);

        const std::string csv = out.str();
        assert(csv.find("requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,bounded_frames,requested_frames,headless,run_config_hash,frame_count,run_status,failure_category,failure_detail,termination_reason,final_world_hash,total_collision_count,peak_collision_count,max_rigid_body_count,max_dynamic_body_count,max_transform_count,avg_update_wall_seconds,p95_update_wall_seconds,avg_render_wall_seconds,p95_render_wall_seconds,avg_frame_wall_seconds,p95_frame_wall_seconds\n") == 0);
        assert(csv.find("fluid,fluid,0,0.008333,1,300,1,777,300,success,,,frame_cap,123456,900,17,250,240,260,0.010000,0.020000,0.003000,0.004000,0.013500,0.024500\n") != std::string::npos);
    }

    void VerifyManifestCsvWriterProducesStableHeaderAndRow()
    {
        simlab::HeadlessRunManifest manifest{};
        manifest.requestedScenarioKey = "gravity";
        manifest.resolvedScenarioKey = "gravity";
        manifest.fallbackUsed = false;
        manifest.fixedDtSeconds = 1.0 / 60.0;
        manifest.boundedFrames = true;
        manifest.requestedFrames = 180;
        manifest.headless = true;
        manifest.runConfigHash = 424242;
        manifest.frameCount = 180;
        manifest.runStatus = "success";
        manifest.failureCategory = "";
        manifest.failureDetail = "";
        manifest.terminationReason = "frame_cap";
        manifest.outputPath = "artifacts/gravity_output.txt";
        manifest.metricsPath = "artifacts/gravity_metrics.csv";
        manifest.summaryPath = "artifacts/gravity_summary.csv";
        manifest.batchIndexPath = "artifacts/batch_index.csv";
        manifest.batchIndexAppendStatus = "appended";
        manifest.batchIndexFailureCategory = "";
        manifest.outputWriteStatus = "written";
        manifest.outputFailureCategory = "";
        manifest.metricsWriteStatus = "written";
        manifest.metricsFailureCategory = "";
        manifest.summaryWriteStatus = "written";
        manifest.summaryFailureCategory = "";
        manifest.manifestWriteStatus = "written";
        manifest.manifestFailureCategory = "";
        manifest.startupFailureSummaryWriteStatus = "not_applicable";
        manifest.startupFailureSummaryFailureCategory = "";
        manifest.startupFailureManifestWriteStatus = "not_applicable";
        manifest.startupFailureManifestFailureCategory = "";
        manifest.exitCode = 0;
        manifest.exitClassification = "success_exit";
        manifest.timestampUtc = "2026-04-08T04:00:00Z";
        manifest.gitCommit = "0123456789abcdef0123456789abcdef01234567";
        manifest.gitDirty = true;
        manifest.buildType = "Debug";

        std::ostringstream out;
        simlab::WriteHeadlessRunManifestCsvHeader(out);
        simlab::WriteHeadlessRunManifestCsvRow(out, manifest);

        const std::string csv = out.str();
        assert(csv.find("requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,bounded_frames,requested_frames,headless,run_config_hash,frame_count,run_status,failure_category,failure_detail,termination_reason,output_path,metrics_path,summary_path,batch_index_path,batch_index_append_status,batch_index_failure_category,output_write_status,output_failure_category,metrics_write_status,metrics_failure_category,summary_write_status,summary_failure_category,manifest_write_status,manifest_failure_category,startup_failure_summary_write_status,startup_failure_summary_failure_category,startup_failure_manifest_write_status,startup_failure_manifest_failure_category,exit_code,exit_classification,timestamp_utc,git_commit,git_dirty,build_type\n") == 0);
        assert(csv.find("gravity,gravity,0,0.016667,1,180,1,424242,180,success,,,frame_cap,artifacts/gravity_output.txt,artifacts/gravity_metrics.csv,artifacts/gravity_summary.csv,artifacts/batch_index.csv,appended,,written,,written,,written,,written,,not_applicable,,not_applicable,,0,success_exit,2026-04-08T04:00:00Z,0123456789abcdef0123456789abcdef01234567,1,Debug\n") != std::string::npos);
    }

    void VerifyRunConfigHashChangesWhenInputsChange()
    {
        const auto base = simlab::HashHeadlessRunConfig("gravity", 1.0 / 60.0, 300, true);
        assert(base != 0u);
        assert(base == simlab::HashHeadlessRunConfig("gravity", 1.0 / 60.0, 300, true));
        assert(base != simlab::HashHeadlessRunConfig("gravity", 1.0 / 120.0, 300, true));
        assert(base != simlab::HashHeadlessRunConfig("gravity", 1.0 / 60.0, 600, true));
        assert(base != simlab::HashHeadlessRunConfig("gravity", 1.0 / 60.0, 300, false));
        assert(base != simlab::HashHeadlessRunConfig("fluid", 1.0 / 60.0, 300, true));
    }

    void VerifyUnboundedFrameMetadataSerializesExplicitly()
    {
        simlab::HeadlessRunSummary summary{};
        summary.requestedScenarioKey = "gravity";
        summary.resolvedScenarioKey = "gravity";
        summary.fallbackUsed = false;
        summary.fixedDtSeconds = 1.0 / 60.0;
        summary.boundedFrames = false;
        summary.requestedFrames = 0;
        summary.headless = true;
        summary.runConfigHash = 111;
        summary.frameCount = 12;
        summary.runStatus = "success";
        summary.failureCategory = "";
        summary.terminationReason = "unbounded_headless_default";

        std::ostringstream summaryOut;
        simlab::WriteHeadlessRunSummaryCsvHeader(summaryOut);
        simlab::WriteHeadlessRunSummaryCsvRow(summaryOut, summary);
        assert(summaryOut.str().find("gravity,gravity,0,0.016667,0,0,1,111,12,success,,,unbounded_headless_default,") != std::string::npos);

        simlab::HeadlessRunManifest manifest{};
        manifest.requestedScenarioKey = "gravity";
        manifest.resolvedScenarioKey = "gravity";
        manifest.fallbackUsed = false;
        manifest.fixedDtSeconds = 1.0 / 60.0;
        manifest.boundedFrames = false;
        manifest.requestedFrames = 0;
        manifest.headless = true;
        manifest.runConfigHash = 111;
        manifest.frameCount = 12;
        manifest.runStatus = "success";
        manifest.failureCategory = "";
        manifest.terminationReason = "unbounded_headless_default";
        manifest.outputPath = "out.txt";
        manifest.metricsPath = "metrics.csv";
        manifest.summaryPath = "summary.csv";
        manifest.batchIndexPath = "";
        manifest.batchIndexAppendStatus = "not_requested";
        manifest.batchIndexFailureCategory = "";
        manifest.outputWriteStatus = "written";
        manifest.outputFailureCategory = "";
        manifest.metricsWriteStatus = "written";
        manifest.metricsFailureCategory = "";
        manifest.summaryWriteStatus = "written";
        manifest.summaryFailureCategory = "";
        manifest.manifestWriteStatus = "written";
        manifest.manifestFailureCategory = "";
        manifest.startupFailureSummaryWriteStatus = "not_applicable";
        manifest.startupFailureSummaryFailureCategory = "";
        manifest.startupFailureManifestWriteStatus = "not_applicable";
        manifest.startupFailureManifestFailureCategory = "";
        manifest.exitCode = 0;
        manifest.exitClassification = "success_exit";
        manifest.timestampUtc = "2026-04-08T04:00:00Z";
        manifest.gitCommit = "0123456789abcdef0123456789abcdef01234567";
        manifest.gitDirty = false;
        manifest.buildType = "Debug";

        std::ostringstream manifestOut;
        simlab::WriteHeadlessRunManifestCsvHeader(manifestOut);
        simlab::WriteHeadlessRunManifestCsvRow(manifestOut, manifest);
        assert(manifestOut.str().find("gravity,gravity,0,0.016667,0,0,1,111,12,success,,,unbounded_headless_default,out.txt,metrics.csv,summary.csv,,not_requested,,written,,written,,written,,written,,not_applicable,,not_applicable,,0,success_exit,") != std::string::npos);
    }

    void VerifyManifestCsvWriterSerializesBatchIndexWriteFailures()
    {
        simlab::HeadlessRunManifest manifest{};
        manifest.requestedScenarioKey = "gravity";
        manifest.resolvedScenarioKey = "gravity";
        manifest.fallbackUsed = false;
        manifest.fixedDtSeconds = 1.0 / 60.0;
        manifest.boundedFrames = true;
        manifest.requestedFrames = 2;
        manifest.headless = true;
        manifest.runConfigHash = 222;
        manifest.frameCount = 2;
        manifest.runStatus = "success";
        manifest.failureCategory = "";
        manifest.terminationReason = "frame_cap";
        manifest.outputPath = "out.txt";
        manifest.metricsPath = "metrics.csv";
        manifest.summaryPath = "summary.csv";
        manifest.batchIndexPath = "/dev/full";
        manifest.batchIndexAppendStatus = "append_failed";
        manifest.batchIndexFailureCategory = "batch_index_write_failed";
        manifest.outputWriteStatus = "written";
        manifest.outputFailureCategory = "";
        manifest.metricsWriteStatus = "written";
        manifest.metricsFailureCategory = "";
        manifest.summaryWriteStatus = "written";
        manifest.summaryFailureCategory = "";
        manifest.manifestWriteStatus = "written";
        manifest.manifestFailureCategory = "";
        manifest.startupFailureSummaryWriteStatus = "not_applicable";
        manifest.startupFailureSummaryFailureCategory = "";
        manifest.startupFailureManifestWriteStatus = "not_applicable";
        manifest.startupFailureManifestFailureCategory = "";
        manifest.exitCode = 0;
        manifest.exitClassification = "success_exit";
        manifest.timestampUtc = "2026-04-08T04:00:00Z";
        manifest.gitCommit = "0123456789abcdef0123456789abcdef01234567";
        manifest.gitDirty = false;
        manifest.buildType = "Debug";

        std::ostringstream out;
        simlab::WriteHeadlessRunManifestCsvHeader(out);
        simlab::WriteHeadlessRunManifestCsvRow(out, manifest);
        assert(out.str().find("gravity,gravity,0,0.016667,1,2,1,222,2,success,,,frame_cap,out.txt,metrics.csv,summary.csv,/dev/full,append_failed,batch_index_write_failed,written,,written,,written,,written,,not_applicable,,not_applicable,,0,success_exit,") != std::string::npos);
    }
}

int main()
{
    VerifyFrameMetricsCapturePhysicsState();
    VerifyCsvWriterProducesStableHeaderAndRow();
    VerifySummaryAccumulatorTracksFinalHashAndAggregates();
    VerifySummaryAccumulatorUsesTrueNearestRankPercentiles();
    VerifySummaryCsvWriterProducesStableHeaderAndRow();
    VerifyManifestCsvWriterProducesStableHeaderAndRow();
    VerifyRunConfigHashChangesWhenInputsChange();
    VerifyUnboundedFrameMetadataSerializesExplicitly();
    VerifyManifestCsvWriterSerializesBatchIndexWriteFailures();
    std::cout << "Headless metrics tests passed\n";
    return 0;
}
