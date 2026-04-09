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
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "simlab/HeadlessMetrics.hpp"
#include "simlab/Scenario.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    class RecordingScenario final : public simlab::IScenario
    {
    public:
        void Setup(ecs::World&) override {}
        void Update(ecs::World&, float) override { ++updateCalls; }
        void Render(ecs::World&, std::ostream& out) override
        {
            ++renderCalls;
            out << "rendered\n";
        }

        int updateCalls{0};
        int renderCalls{0};
    };

    physics::PhysicsSystem* SeedPhysicsWorld(ecs::World& world)
    {
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
        return physicsPtr;
    }

    void VerifyFrameMetricsCapturePhysicsState()
    {
        ecs::World world;
        auto* physicsPtr = SeedPhysicsWorld(world);

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

    void VerifyHeadlessFailurePhaseClassification()
    {
        assert(simlab::ClassifyHeadlessFailurePhase("setup", true) == "scenario_setup_failed");
        assert(simlab::ClassifyHeadlessFailurePhase("output_directory_create_failed", true) == "output_directory_create_failed");
        assert(simlab::ClassifyHeadlessFailurePhase("update", false) == "scenario_update_failed");
        assert(simlab::ClassifyHeadlessFailurePhase("world_update", false) == "world_update_failed");
        assert(simlab::ClassifyHeadlessFailurePhase("render", false) == "scenario_render_failed");
        assert(simlab::ClassifyHeadlessFailurePhase("mystery", false) == "runtime_exception");
    }

    void VerifyHeadlessRunOutcomeTrackerDerivesTerminationAndExitMetadata()
    {
        simlab::HeadlessRunOutcomeTracker runtimeFailure{};
        runtimeFailure.MarkRuntimeFailure("world_update", "Test world update failure");
        assert(runtimeFailure.runStatus == "runtime_failure");
        assert(runtimeFailure.failureCategory == "world_update_failed");
        assert(runtimeFailure.failureDetail == "Test world update failure");
        assert(runtimeFailure.exitCode == 1);
        assert(runtimeFailure.exitClassification == "runtime_failure_exit");
        assert(runtimeFailure.DeriveTerminationReason(true, true, false, false) == "runtime_failure");

        simlab::HeadlessRunOutcomeTracker startupFailure{};
        startupFailure.MarkStartupFailure("setup", "Test scenario setup failure");
        assert(startupFailure.runStatus == "startup_failure");
        assert(startupFailure.failureCategory == "scenario_setup_failed");
        assert(startupFailure.failureDetail == "Test scenario setup failure");
        assert(startupFailure.exitCode == 1);
        assert(startupFailure.exitClassification == "startup_failure_exit");

        simlab::HeadlessRunOutcomeTracker boundedSuccess{};
        assert(boundedSuccess.DeriveTerminationReason(true, true, false, false) == "frame_cap");

        simlab::HeadlessRunOutcomeTracker headlessUnbounded{};
        assert(headlessUnbounded.DeriveTerminationReason(false, true, false, false) == "unbounded_headless_default");

        simlab::HeadlessRunOutcomeTracker eofQuit{};
        assert(eofQuit.DeriveTerminationReason(false, false, false, true) == "eof_quit");

        simlab::HeadlessRunOutcomeTracker userQuit{};
        assert(userQuit.DeriveTerminationReason(false, false, true, false) == "user_quit");
    }

    void VerifyHeadlessRunSummaryReportBuilderAppliesSharedMetadata()
    {
        simlab::HeadlessRunSummary aggregate{};
        aggregate.scenarioKey = "gravity";
        aggregate.frameCount = 12;
        aggregate.finalWorldHash = 321;
        aggregate.totalCollisionCount = 44;

        simlab::HeadlessRunReportContext context{};
        context.requestedScenarioKey = "bad_key";
        context.resolvedScenarioKey = "gravity";
        context.fallbackUsed = true;
        context.fixedDtSeconds = 1.0 / 60.0;
        context.boundedFrames = true;
        context.requestedFrames = 12;
        context.headless = true;
        context.runConfigHash = 999;
        context.terminationReason = "frame_cap";

        simlab::HeadlessRunOutcomeTracker outcome{};

        const auto summary = simlab::BuildHeadlessRunSummaryReport(aggregate, context, outcome);
        assert(summary.scenarioKey == "gravity");
        assert(summary.requestedScenarioKey == "bad_key");
        assert(summary.resolvedScenarioKey == "gravity");
        assert(summary.fallbackUsed);
        assert(summary.fixedDtSeconds == 1.0 / 60.0);
        assert(summary.boundedFrames);
        assert(summary.requestedFrames == 12u);
        assert(summary.headless);
        assert(summary.runConfigHash == 999u);
        assert(summary.frameCount == 12u);
        assert(summary.runStatus == "success");
        assert(summary.failureCategory.empty());
        assert(summary.failureDetail.empty());
        assert(summary.terminationReason == "frame_cap");
        assert(summary.finalWorldHash == 321u);
        assert(summary.totalCollisionCount == 44u);
    }

    void VerifyHeadlessRunManifestReportBuilderAppliesSharedMetadata()
    {
        simlab::HeadlessRunReportContext context{};
        context.requestedScenarioKey = "fail_setup";
        context.resolvedScenarioKey = "fail_setup";
        context.fallbackUsed = false;
        context.fixedDtSeconds = 1.0 / 120.0;
        context.boundedFrames = true;
        context.requestedFrames = 2;
        context.headless = true;
        context.runConfigHash = 1234;
        context.terminationReason = "startup_failure";

        simlab::HeadlessRunArtifactReport artifacts{};
        artifacts.outputPath = "";
        artifacts.metricsPath = "";
        artifacts.summaryPath = "/tmp/headless_startup_failure_summary.csv";
        artifacts.batchIndexPath = "";
        artifacts.batchIndexAppendStatus = "not_requested";
        artifacts.batchIndexFailureCategory = "";
        artifacts.outputWriteStatus = "";
        artifacts.outputFailureCategory = "";
        artifacts.metricsWriteStatus = "";
        artifacts.metricsFailureCategory = "";
        artifacts.summaryWriteStatus = "";
        artifacts.summaryFailureCategory = "";
        artifacts.manifestWriteStatus = "written";
        artifacts.manifestFailureCategory = "";
        artifacts.startupFailureSummaryWriteStatus = "written";
        artifacts.startupFailureSummaryFailureCategory = "";
        artifacts.startupFailureManifestWriteStatus = "pending";
        artifacts.startupFailureManifestFailureCategory = "";
        artifacts.timestampUtc = "2026-04-09T04:30:00Z";
        artifacts.gitCommit = "abcdef0123456789abcdef0123456789abcdef01";
        artifacts.gitDirty = false;
        artifacts.buildType = "Debug";

        simlab::HeadlessRunOutcomeTracker outcome{};
        outcome.MarkStartupFailure("setup", "Test scenario setup failure");

        const auto manifest = simlab::BuildHeadlessRunManifestReport(0u, context, artifacts, outcome);
        assert(manifest.scenarioKey == "fail_setup");
        assert(manifest.requestedScenarioKey == "fail_setup");
        assert(manifest.resolvedScenarioKey == "fail_setup");
        assert(!manifest.fallbackUsed);
        assert(manifest.fixedDtSeconds == 1.0 / 120.0);
        assert(manifest.boundedFrames);
        assert(manifest.requestedFrames == 2u);
        assert(manifest.headless);
        assert(manifest.runConfigHash == 1234u);
        assert(manifest.frameCount == 0u);
        assert(manifest.runStatus == "startup_failure");
        assert(manifest.failureCategory == "scenario_setup_failed");
        assert(manifest.failureDetail == "Test scenario setup failure");
        assert(manifest.terminationReason == "startup_failure");
        assert(manifest.summaryPath == "/tmp/headless_startup_failure_summary.csv");
        assert(manifest.batchIndexAppendStatus == "not_requested");
        assert(manifest.manifestWriteStatus == "written");
        assert(manifest.startupFailureSummaryWriteStatus == "written");
        assert(manifest.startupFailureManifestWriteStatus == "pending");
        assert(manifest.exitCode == 1);
        assert(manifest.exitClassification == "startup_failure_exit");
        assert(manifest.timestampUtc == "2026-04-09T04:30:00Z");
        assert(manifest.gitCommit == "abcdef0123456789abcdef0123456789abcdef01");
        assert(!manifest.gitDirty);
        assert(manifest.buildType == "Debug");
    }

    void VerifyNormalHeadlessArtifactReportBuilderAppliesRuntimeFacts()
    {
        simlab::HeadlessRunArtifactReport artifacts{};
        artifacts.outputPath = "/tmp/headless_output.txt";
        artifacts.metricsPath = "/tmp/headless_metrics.csv";
        artifacts.summaryPath = "/tmp/headless_summary.csv";
        artifacts.batchIndexPath = "/tmp/batch.csv";
        artifacts.batchIndexAppendStatus = "appended";
        artifacts.batchIndexFailureCategory = "";
        artifacts.outputWriteStatus = "written";
        artifacts.outputFailureCategory = "";
        artifacts.metricsWriteStatus = "write_failed";
        artifacts.metricsFailureCategory = "metrics_write_failed";
        artifacts.summaryWriteStatus = "written";
        artifacts.summaryFailureCategory = "";
        artifacts.manifestWriteStatus = "written";
        artifacts.manifestFailureCategory = "";
        artifacts.startupFailureSummaryWriteStatus = "not_applicable";
        artifacts.startupFailureSummaryFailureCategory = "";
        artifacts.startupFailureManifestWriteStatus = "not_applicable";
        artifacts.startupFailureManifestFailureCategory = "";
        artifacts.timestampUtc = "2026-04-09T07:00:00Z";
        artifacts.gitCommit = "9999999999999999999999999999999999999999";
        artifacts.gitDirty = true;
        artifacts.buildType = "RelWithDebInfo";

        const auto built = simlab::BuildNormalHeadlessArtifactReport(artifacts,
                                                                     "/tmp/headless_output.txt",
                                                                     "/tmp/headless_metrics.csv",
                                                                     "/tmp/headless_summary.csv",
                                                                     "/tmp/batch.csv",
                                                                     "appended",
                                                                     "",
                                                                     "written",
                                                                     "",
                                                                     "write_failed",
                                                                     "metrics_write_failed",
                                                                     "written",
                                                                     "",
                                                                     "written",
                                                                     "",
                                                                     "not_applicable",
                                                                     "",
                                                                     "not_applicable",
                                                                     "",
                                                                     "2026-04-09T07:00:00Z",
                                                                     "9999999999999999999999999999999999999999",
                                                                     true,
                                                                     "RelWithDebInfo");

        assert(built.outputPath == "/tmp/headless_output.txt");
        assert(built.metricsPath == "/tmp/headless_metrics.csv");
        assert(built.summaryPath == "/tmp/headless_summary.csv");
        assert(built.batchIndexPath == "/tmp/batch.csv");
        assert(built.batchIndexAppendStatus == "appended");
        assert(built.outputWriteStatus == "written");
        assert(built.metricsWriteStatus == "write_failed");
        assert(built.metricsFailureCategory == "metrics_write_failed");
        assert(built.manifestWriteStatus == "written");
        assert(built.startupFailureSummaryWriteStatus == "not_applicable");
        assert(built.startupFailureManifestWriteStatus == "not_applicable");
        assert(built.timestampUtc == "2026-04-09T07:00:00Z");
        assert(built.gitCommit == "9999999999999999999999999999999999999999");
        assert(built.gitDirty);
        assert(built.buildType == "RelWithDebInfo");
    }

    void VerifyRuntimeFramePreparationBuildsConfigAndArtifacts()
    {
        std::ofstream outputStream{"/dev/null"};
        std::ofstream metricsStream{"/dev/null"};
        assert(outputStream.is_open());
        assert(metricsStream.is_open());

        std::string outputWriteStatus{"written"};
        std::string outputFailureCategory;
        std::string metricsWriteStatus{"written"};
        std::string metricsFailureCategory;

        const auto prepared = simlab::PrepareHeadlessRuntimeFrame(
            true,
            true,
            12,
            outputStream,
            metricsStream,
            outputWriteStatus,
            outputFailureCategory,
            metricsWriteStatus,
            metricsFailureCategory);

        assert(prepared.config.headless);
        assert(prepared.config.boundedFrames);
        assert(prepared.config.maxFrames == 12);
        assert(prepared.artifacts.outputStream == static_cast<std::ostream*>(&outputStream));
        assert(prepared.artifacts.metricsStream == static_cast<std::ostream*>(&metricsStream));
        assert(prepared.artifacts.outputWriteStatus == &outputWriteStatus);
        assert(prepared.artifacts.outputFailureCategory == &outputFailureCategory);
        assert(prepared.artifacts.metricsWriteStatus == &metricsWriteStatus);
        assert(prepared.artifacts.metricsFailureCategory == &metricsFailureCategory);
    }

    void VerifyHeadlessRuntimeFrameCoordinatorAdvancesAndCapturesMetrics()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_runtime_frame_unit";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        ecs::World world;
        SeedPhysicsWorld(world);
        RecordingScenario scenario;
        simlab::HeadlessRunSummaryAccumulator accumulator;
        simlab::HeadlessRuntimeFrameState frameState{};
        simlab::HeadlessRuntimeFrameConfig frameConfig{};
        frameConfig.headless = true;
        frameConfig.boundedFrames = true;
        frameConfig.maxFrames = 1;

        std::ofstream headlessOut(tempRoot / "frame_output.txt");
        std::ofstream metricsOut(tempRoot / "frame_metrics.csv");
        assert(headlessOut.is_open());
        assert(metricsOut.is_open());

        std::string outputWriteStatus{"written"};
        std::string outputFailureCategory;
        std::string metricsWriteStatus{"written"};
        std::string metricsFailureCategory;
        simlab::HeadlessRuntimeFrameArtifacts artifacts{};
        artifacts.outputStream = &headlessOut;
        artifacts.metricsStream = &metricsOut;
        artifacts.outputWriteStatus = &outputWriteStatus;
        artifacts.outputFailureCategory = &outputFailureCategory;
        artifacts.metricsWriteStatus = &metricsWriteStatus;
        artifacts.metricsFailureCategory = &metricsFailureCategory;

        std::ostringstream interactiveOut;
        const bool shouldStop = simlab::RunHeadlessRuntimeFrame(world,
                                                                scenario,
                                                                1.0f / 60.0f,
                                                                frameState,
                                                                frameConfig,
                                                                accumulator,
                                                                interactiveOut,
                                                                artifacts,
                                                                [](std::string_view) {});

        assert(shouldStop);
        assert(frameState.frameCounter == 1);
        assert(frameState.simTimeSeconds > 0.0);
        assert(frameState.currentFailurePhase.empty());
        assert(scenario.updateCalls == 1);
        assert(scenario.renderCalls == 1);
        assert(outputWriteStatus == "written");
        assert(outputFailureCategory.empty());
        assert(metricsWriteStatus == "written");
        assert(metricsFailureCategory.empty());
        assert(accumulator.Build("gravity").frameCount == 1u);

        headlessOut.close();
        metricsOut.close();
        std::ifstream renderedIn(tempRoot / "frame_output.txt");
        const std::string rendered((std::istreambuf_iterator<char>(renderedIn)), std::istreambuf_iterator<char>());
        assert(rendered.find("rendered") != std::string::npos);

        std::ifstream metricsIn(tempRoot / "frame_metrics.csv");
        const std::string metricsCsv((std::istreambuf_iterator<char>(metricsIn)), std::istreambuf_iterator<char>());
        assert(!metricsCsv.empty());

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyHeadlessRuntimeFrameCoordinatorPreservesWorldUpdateFailurePhase()
    {
        ecs::World world;
        SeedPhysicsWorld(world);
        RecordingScenario scenario;
        simlab::HeadlessRunSummaryAccumulator accumulator;
        simlab::HeadlessRuntimeFrameState frameState{};
        simlab::HeadlessRuntimeFrameConfig frameConfig{};
        std::ostringstream interactiveOut;
        simlab::HeadlessRuntimeFrameArtifacts artifacts{};

        bool threw = false;
        try
        {
            static_cast<void>(simlab::RunHeadlessRuntimeFrame(world,
                                                              scenario,
                                                              1.0f / 60.0f,
                                                              frameState,
                                                              frameConfig,
                                                              accumulator,
                                                              interactiveOut,
                                                              artifacts,
                                                              [](std::string_view phase) {
                                                                  if (phase == "world_update")
                                                                  {
                                                                      throw std::runtime_error("Injected world update failure");
                                                                  }
                                                              }));
        }
        catch (const std::runtime_error& ex)
        {
            threw = true;
            assert(std::string(ex.what()) == "Injected world update failure");
        }

        assert(threw);
        assert(frameState.currentFailurePhase == "world_update");
        assert(frameState.frameCounter == 0);
        assert(frameState.simTimeSeconds == 0.0);
        assert(scenario.updateCalls == 1);
        assert(scenario.renderCalls == 0);
        assert(accumulator.Build("gravity").frameCount == 0u);
    }

    void VerifyHeadlessRunFinalizationCoordinatorWritesSummaryManifestAndBatchIndex()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_finalize_unit";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        simlab::HeadlessRunSummaryAccumulator accumulator;
        accumulator.AddFrame(simlab::FrameMetrics{.frameIndex = 1,
                                                  .simTimeSeconds = 1.0 / 60.0,
                                                  .worldHash = 77,
                                                  .collisionCount = 2,
                                                  .rigidBodyCount = 4,
                                                  .dynamicBodyCount = 4,
                                                  .transformCount = 4,
                                                  .updateWallSeconds = 0.001000,
                                                  .renderWallSeconds = 0.000500,
                                                  .frameWallSeconds = 0.001600});

        simlab::HeadlessRunReportContext context{};
        context.requestedScenarioKey = "gravity";
        context.resolvedScenarioKey = "gravity";
        context.fixedDtSeconds = 1.0 / 60.0;
        context.boundedFrames = true;
        context.requestedFrames = 1;
        context.headless = true;
        context.runConfigHash = 42;
        context.terminationReason = "frame_cap";

        simlab::HeadlessRunOutcomeTracker outcome{};

        simlab::HeadlessRunArtifactReport artifacts{};
        artifacts.outputPath = (tempRoot / "gravity_output.txt").string();
        artifacts.metricsPath = (tempRoot / "gravity_metrics.csv").string();
        artifacts.summaryPath = (tempRoot / "gravity_summary.csv").string();
        artifacts.batchIndexPath = (tempRoot / "batch_index.csv").string();
        artifacts.batchIndexAppendStatus = "appended";
        artifacts.outputWriteStatus = "written";
        artifacts.metricsWriteStatus = "written";
        artifacts.summaryWriteStatus = "written";
        artifacts.manifestWriteStatus = "written";
        artifacts.startupFailureSummaryWriteStatus = "not_applicable";
        artifacts.startupFailureManifestWriteStatus = "not_applicable";
        artifacts.timestampUtc = "2026-04-09T08:00:00Z";
        artifacts.gitCommit = "abcdef0123456789abcdef0123456789abcdef01";
        artifacts.gitDirty = false;
        artifacts.buildType = "Debug";

        std::ofstream summaryOut(tempRoot / "gravity_summary.csv", std::ios::app);
        std::ofstream manifestOut(tempRoot / "gravity_manifest.csv", std::ios::app);
        assert(summaryOut.is_open());
        assert(manifestOut.is_open());
        simlab::WriteHeadlessRunSummaryCsvHeader(summaryOut);
        simlab::WriteHeadlessRunManifestCsvHeader(manifestOut);
        summaryOut.flush();
        manifestOut.flush();

        const auto result = simlab::FinalizeHeadlessRunReports("gravity",
                                                               accumulator,
                                                               context,
                                                               outcome,
                                                               artifacts,
                                                               &summaryOut,
                                                               &manifestOut);

        assert(result.summary.frameCount == 1u);
        assert(result.summary.finalWorldHash == 77u);
        assert(result.manifest.frameCount == 1u);
        assert(result.manifest.batchIndexAppendStatus == "appended");
        assert(result.manifest.batchIndexFailureCategory.empty());
        assert(result.artifacts.summaryWriteStatus == "written");
        assert(result.artifacts.manifestWriteStatus == "written");

        summaryOut.close();
        manifestOut.close();
        std::ifstream summaryIn(tempRoot / "gravity_summary.csv");
        const std::string summaryCsv((std::istreambuf_iterator<char>(summaryIn)), std::istreambuf_iterator<char>());
        assert(summaryCsv.find("gravity,gravity,0,0.016667,1,1,1,42,1,success") != std::string::npos);

        std::ifstream manifestIn(tempRoot / "gravity_manifest.csv");
        const std::string manifestCsv((std::istreambuf_iterator<char>(manifestIn)), std::istreambuf_iterator<char>());
        assert(manifestCsv.find("gravity,gravity,0,0.016667,1,1,1,42,1,success") != std::string::npos);

        std::ifstream batchIndexIn(tempRoot / "batch_index.csv");
        const std::string batchIndexCsv((std::istreambuf_iterator<char>(batchIndexIn)), std::istreambuf_iterator<char>());
        assert(batchIndexCsv.find("requested_scenario_key,resolved_scenario_key") == 0);
        assert(batchIndexCsv.find("gravity,gravity,0,0.016667,1,1,1,42,1,success") != std::string::npos);

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyHeadlessRunFinalizationCoordinatorRecordsBatchIndexOpenFailure()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_finalize_batch_fail_unit";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        simlab::HeadlessRunSummaryAccumulator accumulator;
        accumulator.AddFrame(simlab::FrameMetrics{.frameIndex = 1,
                                                  .simTimeSeconds = 1.0 / 60.0,
                                                  .worldHash = 88,
                                                  .collisionCount = 1,
                                                  .rigidBodyCount = 2,
                                                  .dynamicBodyCount = 2,
                                                  .transformCount = 2,
                                                  .updateWallSeconds = 0.001000,
                                                  .renderWallSeconds = 0.000500,
                                                  .frameWallSeconds = 0.001600});

        simlab::HeadlessRunReportContext context{};
        context.requestedScenarioKey = "gravity";
        context.resolvedScenarioKey = "gravity";
        context.fixedDtSeconds = 1.0 / 60.0;
        context.boundedFrames = true;
        context.requestedFrames = 1;
        context.headless = true;
        context.runConfigHash = 43;
        context.terminationReason = "frame_cap";

        simlab::HeadlessRunOutcomeTracker outcome{};

        simlab::HeadlessRunArtifactReport artifacts{};
        artifacts.outputPath = (tempRoot / "gravity_output.txt").string();
        artifacts.metricsPath = (tempRoot / "gravity_metrics.csv").string();
        artifacts.summaryPath = (tempRoot / "gravity_summary.csv").string();
        artifacts.batchIndexPath = "/proc/atlascore_finalize_batch_fail.csv";
        artifacts.batchIndexAppendStatus = "appended";
        artifacts.outputWriteStatus = "written";
        artifacts.metricsWriteStatus = "written";
        artifacts.summaryWriteStatus = "written";
        artifacts.manifestWriteStatus = "written";
        artifacts.startupFailureSummaryWriteStatus = "not_applicable";
        artifacts.startupFailureManifestWriteStatus = "not_applicable";
        artifacts.timestampUtc = "2026-04-09T08:05:00Z";
        artifacts.gitCommit = "abcdef0123456789abcdef0123456789abcdef01";
        artifacts.gitDirty = false;
        artifacts.buildType = "Debug";

        std::ofstream summaryOut(tempRoot / "gravity_summary.csv", std::ios::app);
        std::ofstream manifestOut(tempRoot / "gravity_manifest.csv", std::ios::app);
        assert(summaryOut.is_open());
        assert(manifestOut.is_open());
        simlab::WriteHeadlessRunSummaryCsvHeader(summaryOut);
        simlab::WriteHeadlessRunManifestCsvHeader(manifestOut);
        summaryOut.flush();
        manifestOut.flush();

        const auto result = simlab::FinalizeHeadlessRunReports("gravity",
                                                               accumulator,
                                                               context,
                                                               outcome,
                                                               artifacts,
                                                               &summaryOut,
                                                               &manifestOut);

        assert(result.summary.frameCount == 1u);
        assert(result.artifacts.batchIndexAppendStatus == "append_failed");
        assert(result.artifacts.batchIndexFailureCategory == "batch_index_open_failed");
        assert(result.manifest.batchIndexAppendStatus == "append_failed");
        assert(result.manifest.batchIndexFailureCategory == "batch_index_open_failed");

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyHeadlessStartupCoordinatorBootstrapsHealthyHeadlessRun()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_startup_coord_unit";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        ecs::World world;
        RecordingScenario scenario;
        simlab::HeadlessRunOutcomeTracker outcome{};
        simlab::HeadlessStartupCoordinatorConfig config{};
        config.headless = true;
        config.outputPrefix = (tempRoot / "gravity_run").string();
        config.batchIndexPath = (tempRoot / "batch" / "index.csv").string();
        config.requestedScenarioKey = "gravity";
        config.resolvedScenarioKey = "gravity";
        config.fixedDtSeconds = 1.0 / 60.0;
        config.boundedFrames = true;
        config.requestedFrames = 2;
        config.runConfigHash = 55;
        config.startupFailureSummaryPath = (tempRoot / "startup_failure_summary.csv").string();
        config.startupFailureManifestPath = (tempRoot / "startup_failure_manifest.csv").string();
        config.timestampUtc = "2026-04-09T09:00:00Z";
        config.gitCommit = "abcdef0123456789abcdef0123456789abcdef01";
        config.buildType = "Debug";

        auto result = simlab::CoordinateHeadlessStartup(world,
                                                        scenario,
                                                        outcome,
                                                        config,
                                                        [](std::string_view) {});

        assert(outcome.runStatus == "success");
        assert(result.bootstrap.startupFailureCategory.empty());
        assert(result.bootstrap.outputStream.is_open());
        assert(result.bootstrap.metricsStream.is_open());
        assert(result.bootstrap.summaryStream.is_open());
        assert(result.bootstrap.manifestStream.is_open());
        assert(result.bootstrap.batchIndexAppendStatus == "appended");
        assert(result.startupFailureSummaryWriteStatus == "not_applicable");
        assert(result.startupFailureManifestWriteStatus == "not_applicable");
        assert(!result.startupFailureSummaryOpened);
        assert(!result.startupFailureManifestOpened);

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyHeadlessStartupCoordinatorWritesFallbackArtifactsForSetupFailure()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_startup_coord_fail_unit";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        ecs::World world;
        RecordingScenario scenario;
        simlab::HeadlessRunOutcomeTracker outcome{};
        simlab::HeadlessStartupCoordinatorConfig config{};
        config.headless = true;
        config.outputPrefix = (tempRoot / "gravity_run").string();
        config.requestedScenarioKey = "fail_setup";
        config.resolvedScenarioKey = "fail_setup";
        config.fixedDtSeconds = 1.0 / 60.0;
        config.boundedFrames = true;
        config.requestedFrames = 2;
        config.runConfigHash = 56;
        config.startupFailureSummaryPath = (tempRoot / "startup_failure_summary.csv").string();
        config.startupFailureManifestPath = (tempRoot / "startup_failure_manifest.csv").string();
        config.timestampUtc = "2026-04-09T09:05:00Z";
        config.gitCommit = "abcdef0123456789abcdef0123456789abcdef01";
        config.buildType = "Debug";

        auto result = simlab::CoordinateHeadlessStartup(world,
                                                        scenario,
                                                        outcome,
                                                        config,
                                                        [](std::string_view phase) {
                                                            if (phase == "setup")
                                                            {
                                                                throw std::runtime_error("Injected setup failure");
                                                            }
                                                        });

        assert(outcome.runStatus == "startup_failure");
        assert(outcome.failureCategory == "scenario_setup_failed");
        assert(outcome.failureDetail == "Injected setup failure");
        assert(result.bootstrap.outputStream.is_open());
        assert(result.startupFailureSummaryWriteStatus == "written");
        assert(result.startupFailureManifestWriteStatus == "written");
        assert(result.startupFailureSummaryOpened);
        assert(result.startupFailureManifestOpened);

        std::ifstream summaryIn(tempRoot / "startup_failure_summary.csv");
        const std::string summaryCsv((std::istreambuf_iterator<char>(summaryIn)), std::istreambuf_iterator<char>());
        assert(summaryCsv.find("startup_failure") != std::string::npos);

        std::ifstream manifestIn(tempRoot / "startup_failure_manifest.csv");
        const std::string manifestCsv((std::istreambuf_iterator<char>(manifestIn)), std::istreambuf_iterator<char>());
        assert(manifestCsv.find("scenario_setup_failed") != std::string::npos);

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyStartupCoordinatorConfigBuilderAppliesRuntimeFacts()
    {
        const auto config = simlab::BuildHeadlessStartupCoordinatorConfig(
            true,
            "artifacts/gravity_run",
            "artifacts/batch/index.csv",
            "requested_gravity",
            "gravity",
            true,
            1.0 / 60.0,
            true,
            240,
            987654,
            "headless_startup_failure_summary.csv",
            "headless_startup_failure_manifest.csv",
            "2026-04-09T11:00:00Z",
            "abcdef0123456789abcdef0123456789abcdef01",
            false,
            "RelWithDebInfo");

        assert(config.headless);
        assert(config.outputPrefix == "artifacts/gravity_run");
        assert(config.batchIndexPath == "artifacts/batch/index.csv");
        assert(config.requestedScenarioKey == "requested_gravity");
        assert(config.resolvedScenarioKey == "gravity");
        assert(config.fallbackUsed);
        assert(config.fixedDtSeconds == 1.0 / 60.0);
        assert(config.boundedFrames);
        assert(config.requestedFrames == 240u);
        assert(config.runConfigHash == 987654u);
        assert(config.startupFailureSummaryPath == "headless_startup_failure_summary.csv");
        assert(config.startupFailureManifestPath == "headless_startup_failure_manifest.csv");
        assert(config.timestampUtc == "2026-04-09T11:00:00Z");
        assert(config.gitCommit == "abcdef0123456789abcdef0123456789abcdef01");
        assert(!config.gitDirty);
        assert(config.buildType == "RelWithDebInfo");
    }

    void VerifyStartupResultApplicationMovesBootstrapState()
    {
        simlab::HeadlessStartupCoordinatorResult startup{};
        startup.bootstrap.outputPath = "artifacts/run_output.txt";
        startup.bootstrap.metricsPath = "artifacts/run_metrics.csv";
        startup.bootstrap.summaryPath = "artifacts/run_summary.csv";
        startup.bootstrap.manifestPath = "artifacts/run_manifest.csv";
        startup.bootstrap.batchIndexAppendStatus = "append_failed";
        startup.bootstrap.batchIndexFailureCategory = "batch_index_open_failed";
        startup.bootstrap.outputWriteStatus = "written";
        startup.bootstrap.outputFailureCategory = "";
        startup.bootstrap.metricsWriteStatus = "write_failed";
        startup.bootstrap.metricsFailureCategory = "metrics_write_failed";
        startup.bootstrap.summaryWriteStatus = "written";
        startup.bootstrap.summaryFailureCategory = "";
        startup.bootstrap.manifestWriteStatus = "written";
        startup.bootstrap.manifestFailureCategory = "";
        startup.startupFailureSummaryWriteStatus = "written";
        startup.startupFailureSummaryFailureCategory = "";
        startup.startupFailureManifestWriteStatus = "write_failed";
        startup.startupFailureManifestFailureCategory = "startup_failure_manifest_write_failed";
        startup.bootstrap.outputStream.open("/dev/null");
        startup.bootstrap.metricsStream.open("/dev/null");
        startup.bootstrap.summaryStream.open("/dev/null");
        startup.bootstrap.manifestStream.open("/dev/null");

        auto applied = simlab::ApplyHeadlessStartupResult(std::move(startup));

        assert(applied.outputPath == "artifacts/run_output.txt");
        assert(applied.metricsPath == "artifacts/run_metrics.csv");
        assert(applied.summaryPath == "artifacts/run_summary.csv");
        assert(applied.manifestPath == "artifacts/run_manifest.csv");
        assert(applied.batchIndexAppendStatus == "append_failed");
        assert(applied.batchIndexFailureCategory == "batch_index_open_failed");
        assert(applied.outputWriteStatus == "written");
        assert(applied.metricsWriteStatus == "write_failed");
        assert(applied.metricsFailureCategory == "metrics_write_failed");
        assert(applied.summaryWriteStatus == "written");
        assert(applied.manifestWriteStatus == "written");
        assert(applied.startupFailureSummaryWriteStatus == "written");
        assert(applied.startupFailureManifestWriteStatus == "write_failed");
        assert(applied.startupFailureManifestFailureCategory == "startup_failure_manifest_write_failed");
        assert(applied.outputStream.is_open());
        assert(applied.metricsStream.is_open());
        assert(applied.summaryStream.is_open());
        assert(applied.manifestStream.is_open());
    }

    void VerifyHeadlessLocalStateBuilderSeedsDefaults()
    {
        const auto state = simlab::BuildHeadlessLocalState("artifacts/batch/index.csv");
        assert(state.batchIndexAppendStatus == "appended");
        assert(state.batchIndexFailureCategory.empty());
        assert(state.outputPath.empty());
        assert(state.metricsPath.empty());
        assert(state.summaryPath.empty());
        assert(state.manifestPath.empty());
        assert(state.outputWriteStatus.empty());
        assert(state.metricsWriteStatus.empty());
        assert(state.summaryWriteStatus.empty());
        assert(state.manifestWriteStatus.empty());
        assert(state.startupFailureSummaryWriteStatus == "not_applicable");
        assert(state.startupFailureManifestWriteStatus == "not_applicable");
        assert(state.startupFailureSummaryPath == "headless_startup_failure_summary.csv");
        assert(state.startupFailureManifestPath == "headless_startup_failure_manifest.csv");
        assert(state.outcome.runStatus == "success");
        assert(state.outcome.exitCode == 0);

        const auto noBatchState = simlab::BuildHeadlessLocalState("");
        assert(noBatchState.batchIndexAppendStatus == "not_requested");
    }

    void VerifyStartupLoggingPreparationBuildsResolvedPaths()
    {
        simlab::HeadlessStartupCoordinatorResult startup{};
        startup.bootstrap.outputPath = "artifacts/run_output.txt";
        startup.bootstrap.metricsPath = "artifacts/run_metrics.csv";
        startup.bootstrap.summaryPath = "artifacts/run_summary.csv";
        startup.bootstrap.manifestPath = "artifacts/run_manifest.csv";

        const auto prepared = simlab::PrepareHeadlessStartupLogging(
            startup,
            "artifacts/batch/index.csv",
            "headless_startup_failure_summary.csv",
            "headless_startup_failure_manifest.csv");

        assert(prepared.outputPath == "artifacts/run_output.txt");
        assert(prepared.metricsPath == "artifacts/run_metrics.csv");
        assert(prepared.summaryPath == "artifacts/run_summary.csv");
        assert(prepared.manifestPath == "artifacts/run_manifest.csv");
        assert(prepared.batchIndexPath == "artifacts/batch/index.csv");
        assert(prepared.startupFailureSummaryPath == "headless_startup_failure_summary.csv");
        assert(prepared.startupFailureManifestPath == "headless_startup_failure_manifest.csv");
    }

    void VerifyHeadlessStartupLoggingReportsPathsAndStartupFailures()
    {
        core::Logger logger;
        auto sink = std::make_shared<std::ostringstream>();
        logger.SetOutput(sink);

        simlab::HeadlessStartupCoordinatorResult startup{};
        startup.bootstrap.outputPath = "artifacts/run_output.txt";
        startup.bootstrap.metricsPath = "artifacts/run_metrics.csv";
        startup.bootstrap.summaryPath = "artifacts/run_summary.csv";
        startup.bootstrap.manifestPath = "artifacts/run_manifest.csv";
        startup.bootstrap.outputStream.open("/dev/null");
        startup.bootstrap.metricsStream.open("/dev/null");
        startup.bootstrap.summaryStream.open("/dev/null");
        startup.bootstrap.manifestStream.open("/dev/null");
        startup.bootstrap.startupFailureCategory = "metrics_file_open_failed";
        startup.bootstrap.batchIndexFailureCategory = "batch_index_open_failed";
        startup.startupFailureSummaryOpened = false;
        startup.startupFailureManifestOpened = false;

        simlab::HeadlessRunOutcomeTracker outcome{};
        outcome.MarkStartupFailure("setup", "Test scenario setup failure");

        const auto prepared = simlab::PrepareHeadlessStartupLogging(
            startup,
            "artifacts/batch/index.csv",
            "headless_startup_failure_summary.csv",
            "headless_startup_failure_manifest.csv");

        simlab::LogHeadlessStartupMessages(logger,
                                          startup,
                                          outcome,
                                          prepared.outputPath,
                                          prepared.metricsPath,
                                          prepared.summaryPath,
                                          prepared.manifestPath,
                                          prepared.batchIndexPath,
                                          prepared.startupFailureSummaryPath,
                                          prepared.startupFailureManifestPath);

        const auto log = sink->str();
        assert(log.find("Failed to open artifacts/run_metrics.csv") != std::string::npos);
        assert(log.find("Failed to create batch index directory: artifacts/batch") != std::string::npos);
        assert(log.find("Headless output path:") != std::string::npos);
        assert(log.find("Headless metrics path:") != std::string::npos);
        assert(log.find("Headless summary path:") != std::string::npos);
        assert(log.find("Headless manifest path:") != std::string::npos);
        assert(log.find("Failed to open startup failure summary: headless_startup_failure_summary.csv") != std::string::npos);
        assert(log.find("Failed to open startup failure manifest: headless_startup_failure_manifest.csv") != std::string::npos);
    }

    void VerifyFinalizationLoggingPreparationResolvesBatchIndexPath()
    {
        const auto prepared = simlab::PrepareHeadlessFinalizationLogging("artifacts/batch/index.csv");
        assert(prepared.batchIndexPath.find("artifacts/batch/index.csv") != std::string::npos);

        const auto emptyPrepared = simlab::PrepareHeadlessFinalizationLogging("");
        assert(emptyPrepared.batchIndexPath.empty());
    }

    void VerifyHeadlessFinalizationLoggingReportsBatchIndexOpenFailure()
    {
        core::Logger logger;
        auto sink = std::make_shared<std::ostringstream>();
        logger.SetOutput(sink);

        const auto prepared = simlab::PrepareHeadlessFinalizationLogging("/tmp/atlascore_batch/index.csv");
        simlab::LogHeadlessFinalizationMessages(logger,
                                               prepared.batchIndexPath,
                                               "batch_index_open_failed");

        const auto log = sink->str();
        assert(log.find("Failed to open batch index: /tmp/atlascore_batch/index.csv") != std::string::npos);
    }

    void VerifyHeadlessFinalizationPreparationBuildsReportContextAndArtifacts()
    {
        simlab::HeadlessRunOutcomeTracker outcome{};
        const auto prepared = simlab::PrepareHeadlessRunFinalization(
            outcome,
            "requested_gravity",
            "gravity",
            true,
            1.0 / 60.0,
            true,
            180,
            true,
            424242,
            true,
            false,
            "/tmp/headless_output.txt",
            "/tmp/headless_metrics.csv",
            "/tmp/headless_summary.csv",
            "/tmp/batch.csv",
            "appended",
            "",
            "written",
            "",
            "written",
            "",
            "written",
            "",
            "written",
            "",
            "not_applicable",
            "",
            "not_applicable",
            "",
            "2026-04-09T10:00:00Z",
            "abcdef0123456789abcdef0123456789abcdef01",
            false,
            "Debug");

        assert(prepared.context.requestedScenarioKey == "requested_gravity");
        assert(prepared.context.resolvedScenarioKey == "gravity");
        assert(prepared.context.fallbackUsed);
        assert(prepared.context.fixedDtSeconds == 1.0 / 60.0);
        assert(prepared.context.boundedFrames);
        assert(prepared.context.requestedFrames == 180u);
        assert(prepared.context.headless);
        assert(prepared.context.runConfigHash == 424242u);
        assert(prepared.context.terminationReason == "frame_cap");
        assert(prepared.artifacts.outputPath == "/tmp/headless_output.txt");
        assert(prepared.artifacts.metricsPath == "/tmp/headless_metrics.csv");
        assert(prepared.artifacts.summaryPath == "/tmp/headless_summary.csv");
        assert(prepared.artifacts.batchIndexPath == "/tmp/batch.csv");
        assert(prepared.artifacts.batchIndexAppendStatus == "appended");
        assert(prepared.artifacts.outputWriteStatus == "written");
        assert(prepared.artifacts.metricsWriteStatus == "written");
        assert(prepared.artifacts.summaryWriteStatus == "written");
        assert(prepared.artifacts.manifestWriteStatus == "written");
        assert(prepared.artifacts.timestampUtc == "2026-04-09T10:00:00Z");
        assert(prepared.artifacts.gitCommit == "abcdef0123456789abcdef0123456789abcdef01");
        assert(!prepared.artifacts.gitDirty);
        assert(prepared.artifacts.buildType == "Debug");
    }

    void VerifyFinalizationResultApplicationCopiesArtifactStatuses()
    {
        simlab::HeadlessRunFinalizationResult finalization{};
        finalization.artifacts.summaryWriteStatus = "write_failed";
        finalization.artifacts.summaryFailureCategory = "summary_write_failed";
        finalization.artifacts.manifestWriteStatus = "written";
        finalization.artifacts.manifestFailureCategory = "";
        finalization.artifacts.batchIndexAppendStatus = "append_failed";
        finalization.artifacts.batchIndexFailureCategory = "batch_index_open_failed";

        const auto applied = simlab::ApplyHeadlessRunFinalizationResult(finalization);

        assert(applied.summaryWriteStatus == "write_failed");
        assert(applied.summaryFailureCategory == "summary_write_failed");
        assert(applied.manifestWriteStatus == "written");
        assert(applied.manifestFailureCategory.empty());
        assert(applied.batchIndexAppendStatus == "append_failed");
        assert(applied.batchIndexFailureCategory == "batch_index_open_failed");
    }

    void VerifyHeadlessArtifactIoHelpersReportSuccessAndFailure()
    {
        std::string writeStatus;
        std::string failureCategory;
        simlab::MarkHeadlessArtifactOpened(writeStatus);
        assert(writeStatus == "written");

        std::ostringstream goodOut;
        goodOut << "ok";
        simlab::FinalizeHeadlessArtifactWrite(goodOut, writeStatus, failureCategory, "summary_write_failed");
        assert(writeStatus == "written");
        assert(failureCategory.empty());

        std::ostringstream badOut;
        badOut.setstate(std::ios::badbit);
        simlab::FinalizeHeadlessArtifactWrite(badOut, writeStatus, failureCategory, "summary_write_failed");
        assert(writeStatus == "write_failed");
        assert(failureCategory == "summary_write_failed");

        std::string appendStatus{"appended"};
        std::string appendFailureCategory;
        std::ostringstream badAppendOut;
        badAppendOut.setstate(std::ios::badbit);
        simlab::FinalizeHeadlessBatchAppend(badAppendOut, appendStatus, appendFailureCategory, "batch_index_write_failed");
        assert(appendStatus == "append_failed");
        assert(appendFailureCategory == "batch_index_write_failed");
    }

    void VerifyHeadlessBatchIndexAppendCoordinatorWritesHeaderAndRows()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_batch_index_unit";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        const auto batchIndexPath = tempRoot / "batch.csv";

        simlab::HeadlessRunManifest manifest{};
        manifest.requestedScenarioKey = "gravity";
        manifest.resolvedScenarioKey = "gravity";
        manifest.fallbackUsed = false;
        manifest.fixedDtSeconds = 1.0 / 60.0;
        manifest.boundedFrames = true;
        manifest.requestedFrames = 3;
        manifest.headless = true;
        manifest.runConfigHash = 777;
        manifest.frameCount = 3;
        manifest.runStatus = "success";
        manifest.terminationReason = "frame_cap";
        manifest.outputPath = "out1";
        manifest.metricsPath = "metrics1";
        manifest.summaryPath = "summary1";
        manifest.batchIndexPath = batchIndexPath.string();
        manifest.batchIndexAppendStatus = "appended";
        manifest.outputWriteStatus = "written";
        manifest.metricsWriteStatus = "written";
        manifest.summaryWriteStatus = "written";
        manifest.manifestWriteStatus = "written";
        manifest.startupFailureSummaryWriteStatus = "not_applicable";
        manifest.startupFailureManifestWriteStatus = "not_applicable";
        manifest.exitCode = 0;
        manifest.exitClassification = "success_exit";
        manifest.timestampUtc = "2026-04-09T05:00:00Z";
        manifest.gitCommit = "1111111111111111111111111111111111111111";
        manifest.buildType = "Debug";

        std::string appendStatus{"appended"};
        std::string failureCategory;
        simlab::AppendHeadlessManifestToBatchIndex(batchIndexPath, manifest, appendStatus, failureCategory);
        assert(appendStatus == "appended");
        assert(failureCategory.empty());

        manifest.outputPath = "out2";
        manifest.metricsPath = "metrics2";
        manifest.summaryPath = "summary2";
        manifest.timestampUtc = "2026-04-09T05:01:00Z";
        appendStatus = "appended";
        failureCategory.clear();
        simlab::AppendHeadlessManifestToBatchIndex(batchIndexPath, manifest, appendStatus, failureCategory);
        assert(appendStatus == "appended");
        assert(failureCategory.empty());

        std::ifstream in(batchIndexPath);
        assert(in.is_open());
        const std::string csv((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(csv.find("requested_scenario_key,resolved_scenario_key") == 0);
        assert(csv.find("out1") != std::string::npos);
        assert(csv.find("out2") != std::string::npos);
        assert(csv.find("requested_scenario_key,resolved_scenario_key", 1) == std::string::npos);

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyHeadlessBatchIndexAppendCoordinatorClassifiesOpenFailure()
    {
        simlab::HeadlessRunManifest manifest{};
        std::string appendStatus{"appended"};
        std::string failureCategory;
        simlab::AppendHeadlessManifestToBatchIndex("/proc/atlascore_batch_index.csv", manifest, appendStatus, failureCategory);
        assert(appendStatus == "append_failed");
        assert(failureCategory == "batch_index_open_failed");
    }

    void VerifyHeadlessBatchIndexAppendCoordinatorClassifiesWriteFailure()
    {
        simlab::HeadlessRunManifest manifest{};
        std::string appendStatus{"appended"};
        std::string failureCategory;
        simlab::AppendHeadlessManifestToBatchIndex("/dev/full", manifest, appendStatus, failureCategory);
        assert(appendStatus == "append_failed");
        assert(failureCategory == "batch_index_write_failed");
    }

    void VerifyHeadlessArtifactBootstrapCoordinatorOpensArtifactsAndWritesHeaders()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_bootstrap_unit";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        auto result = simlab::BootstrapHeadlessArtifacts(tempRoot / "gravity_run", tempRoot / "batch" / "index.csv");
        assert(result.startupFailureCategory.empty());
        assert(result.outputPath == (tempRoot / "gravity_run_output.txt").string());
        assert(result.metricsPath == (tempRoot / "gravity_run_metrics.csv").string());
        assert(result.summaryPath == (tempRoot / "gravity_run_summary.csv").string());
        assert(result.manifestPath == (tempRoot / "gravity_run_manifest.csv").string());
        assert(result.outputStream.is_open());
        assert(result.metricsStream.is_open());
        assert(result.summaryStream.is_open());
        assert(result.manifestStream.is_open());
        assert(result.batchIndexAppendStatus == "appended");
        assert(result.batchIndexFailureCategory.empty());
        assert(result.outputWriteStatus == "written");
        assert(result.metricsWriteStatus == "written");
        assert(result.summaryWriteStatus == "written");
        assert(result.manifestWriteStatus == "written");

        result.metricsStream.close();
        result.summaryStream.close();
        result.manifestStream.close();

        std::ifstream metricsIn(tempRoot / "gravity_run_metrics.csv");
        assert(metricsIn.is_open());
        const std::string metricsCsv((std::istreambuf_iterator<char>(metricsIn)), std::istreambuf_iterator<char>());
        assert(metricsCsv.find("frame,sim_time_seconds,world_hash") == 0);

        std::ifstream summaryIn(tempRoot / "gravity_run_summary.csv");
        assert(summaryIn.is_open());
        const std::string summaryCsv((std::istreambuf_iterator<char>(summaryIn)), std::istreambuf_iterator<char>());
        assert(summaryCsv.find("requested_scenario_key,resolved_scenario_key") == 0);

        std::ifstream manifestIn(tempRoot / "gravity_run_manifest.csv");
        assert(manifestIn.is_open());
        const std::string manifestCsv((std::istreambuf_iterator<char>(manifestIn)), std::istreambuf_iterator<char>());
        assert(manifestCsv.find("requested_scenario_key,resolved_scenario_key") == 0);

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyHeadlessArtifactBootstrapCoordinatorClassifiesBatchIndexDirectoryFailureSeparately()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_bootstrap_batch_fail";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        auto result = simlab::BootstrapHeadlessArtifacts(tempRoot / "gravity_run",
                                                         "/proc/atlascore_headless_bootstrap_batch/index.csv");
        assert(result.startupFailureCategory.empty());
        assert(result.outputStream.is_open());
        assert(result.metricsStream.is_open());
        assert(result.summaryStream.is_open());
        assert(result.manifestStream.is_open());
        assert(result.batchIndexAppendStatus == "append_failed");
        assert(result.batchIndexFailureCategory == "batch_index_open_failed");

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyHeadlessArtifactBootstrapCoordinatorClassifiesOutputDirectoryFailure()
    {
        auto result = simlab::BootstrapHeadlessArtifacts("/proc/atlascore_headless_bootstrap/run", {});
        assert(result.startupFailureCategory == "output_directory_create_failed");
        assert(!result.outputStream.is_open());
        assert(!result.metricsStream.is_open());
        assert(!result.summaryStream.is_open());
        assert(!result.manifestStream.is_open());
        assert(result.batchIndexAppendStatus == "not_requested");
        assert(result.batchIndexFailureCategory.empty());
    }

    void VerifyHeadlessStartupFailureArtifactCoordinatorWritesSummaryAndManifest()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_startup_failure_unit";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        simlab::HeadlessRunReportContext context{};
        context.requestedScenarioKey = "fail_setup";
        context.resolvedScenarioKey = "fail_setup";
        context.fixedDtSeconds = 1.0 / 120.0;
        context.boundedFrames = true;
        context.requestedFrames = 2;
        context.headless = true;
        context.runConfigHash = 1234;
        context.terminationReason = "startup_failure";

        simlab::HeadlessRunOutcomeTracker outcome{};
        outcome.MarkStartupFailure("setup", "Test scenario setup failure");

        const auto result = simlab::WriteHeadlessStartupFailureArtifacts(tempRoot / "failure_summary.csv",
                                                                         tempRoot / "failure_manifest.csv",
                                                                         "fail_setup",
                                                                         context,
                                                                         outcome,
                                                                         "scenario_setup_failed",
                                                                         "2026-04-09T06:00:00Z",
                                                                         "feedfacefeedfacefeedfacefeedfacefeedface",
                                                                         false,
                                                                         "Debug");

        assert(result.summaryOpened);
        assert(result.manifestOpened);
        assert(result.summary.failureCategory == "scenario_setup_failed");
        assert(result.manifest.failureCategory == "scenario_setup_failed");
        assert(result.manifest.startupFailureSummaryWriteStatus == "written");
        assert(result.manifest.startupFailureManifestWriteStatus == "written");

        std::ifstream summaryIn(tempRoot / "failure_summary.csv");
        assert(summaryIn.is_open());
        const std::string summaryCsv((std::istreambuf_iterator<char>(summaryIn)), std::istreambuf_iterator<char>());
        assert(!summaryCsv.empty());

        std::ifstream manifestIn(tempRoot / "failure_manifest.csv");
        assert(manifestIn.is_open());
        const std::string manifestCsv((std::istreambuf_iterator<char>(manifestIn)), std::istreambuf_iterator<char>());
        assert(!manifestCsv.empty());

        std::filesystem::remove_all(tempRoot, ec);
    }

    void VerifyHeadlessStartupFailureArtifactCoordinatorReportsSummaryOpenFailure()
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "atlascore_headless_startup_failure_open_fail";
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        assert(!ec);

        simlab::HeadlessRunReportContext context{};
        context.requestedScenarioKey = "fail_setup";
        context.resolvedScenarioKey = "fail_setup";
        context.fixedDtSeconds = 1.0 / 120.0;
        context.boundedFrames = true;
        context.requestedFrames = 2;
        context.headless = true;
        context.runConfigHash = 1234;
        context.terminationReason = "startup_failure";

        simlab::HeadlessRunOutcomeTracker outcome{};
        outcome.MarkStartupFailure("setup", "Test scenario setup failure");

        const auto result = simlab::WriteHeadlessStartupFailureArtifacts("/proc/atlascore_startup_failure_summary.csv",
                                                                         tempRoot / "failure_manifest.csv",
                                                                         "fail_setup",
                                                                         context,
                                                                         outcome,
                                                                         "scenario_setup_failed",
                                                                         "2026-04-09T06:00:00Z",
                                                                         "feedfacefeedfacefeedfacefeedfacefeedface",
                                                                         false,
                                                                         "Debug");

        assert(!result.summaryOpened);
        assert(result.manifestOpened);
        assert(result.manifest.startupFailureSummaryWriteStatus == "not_applicable");
        assert(result.manifest.startupFailureManifestWriteStatus == "written");

        std::filesystem::remove_all(tempRoot, ec);
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
    VerifyHeadlessFailurePhaseClassification();
    VerifyHeadlessRunOutcomeTrackerDerivesTerminationAndExitMetadata();
    VerifyHeadlessRunSummaryReportBuilderAppliesSharedMetadata();
    VerifyHeadlessRunManifestReportBuilderAppliesSharedMetadata();
    VerifyNormalHeadlessArtifactReportBuilderAppliesRuntimeFacts();
    VerifyRuntimeFramePreparationBuildsConfigAndArtifacts();
    VerifyHeadlessRuntimeFrameCoordinatorAdvancesAndCapturesMetrics();
    VerifyHeadlessRuntimeFrameCoordinatorPreservesWorldUpdateFailurePhase();
    VerifyHeadlessRunFinalizationCoordinatorWritesSummaryManifestAndBatchIndex();
    VerifyHeadlessRunFinalizationCoordinatorRecordsBatchIndexOpenFailure();
    VerifyHeadlessStartupCoordinatorBootstrapsHealthyHeadlessRun();
    VerifyHeadlessStartupCoordinatorWritesFallbackArtifactsForSetupFailure();
    VerifyStartupCoordinatorConfigBuilderAppliesRuntimeFacts();
    VerifyStartupResultApplicationMovesBootstrapState();
    VerifyHeadlessLocalStateBuilderSeedsDefaults();
    VerifyStartupLoggingPreparationBuildsResolvedPaths();
    VerifyHeadlessStartupLoggingReportsPathsAndStartupFailures();
    VerifyFinalizationLoggingPreparationResolvesBatchIndexPath();
    VerifyHeadlessFinalizationLoggingReportsBatchIndexOpenFailure();
    VerifyHeadlessFinalizationPreparationBuildsReportContextAndArtifacts();
    VerifyFinalizationResultApplicationCopiesArtifactStatuses();
    VerifyHeadlessArtifactIoHelpersReportSuccessAndFailure();
    VerifyHeadlessBatchIndexAppendCoordinatorWritesHeaderAndRows();
    VerifyHeadlessBatchIndexAppendCoordinatorClassifiesOpenFailure();
    VerifyHeadlessBatchIndexAppendCoordinatorClassifiesWriteFailure();
    VerifyHeadlessArtifactBootstrapCoordinatorOpensArtifactsAndWritesHeaders();
    VerifyHeadlessArtifactBootstrapCoordinatorClassifiesBatchIndexDirectoryFailureSeparately();
    VerifyHeadlessArtifactBootstrapCoordinatorClassifiesOutputDirectoryFailure();
    VerifyHeadlessStartupFailureArtifactCoordinatorWritesSummaryAndManifest();
    VerifyHeadlessStartupFailureArtifactCoordinatorReportsSummaryOpenFailure();
    VerifyRunConfigHashChangesWhenInputsChange();
    VerifyUnboundedFrameMetadataSerializesExplicitly();
    VerifyManifestCsvWriterSerializesBatchIndexWriteFailures();
    std::cout << "Headless metrics tests passed\n";
    return 0;
}
