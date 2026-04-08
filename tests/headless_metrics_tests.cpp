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

    void VerifySummaryAccumulatorTracksFinalHashAndPercentiles()
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
        assert(summary.p95UpdateWallSeconds == 0.003000);
        assert(summary.avgRenderWallSeconds == 0.0005333333333333334);
        assert(summary.p95RenderWallSeconds == 0.000700);
        assert(summary.avgFrameWallSeconds == 0.0026999999999999997);
        assert(summary.p95FrameWallSeconds == 0.003900);
    }

    void VerifySummaryCsvWriterProducesStableHeaderAndRow()
    {
        simlab::HeadlessRunSummary summary{};
        summary.scenarioKey = "fluid";
        summary.frameCount = 300;
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
        assert(csv.find("scenario_key,frame_count,final_world_hash,total_collision_count,peak_collision_count,max_rigid_body_count,max_dynamic_body_count,max_transform_count,avg_update_wall_seconds,p95_update_wall_seconds,avg_render_wall_seconds,p95_render_wall_seconds,avg_frame_wall_seconds,p95_frame_wall_seconds\n") == 0);
        assert(csv.find("fluid,300,123456,900,17,250,240,260,0.010000,0.020000,0.003000,0.004000,0.013500,0.024500\n") != std::string::npos);
    }
}

int main()
{
    VerifyFrameMetricsCapturePhysicsState();
    VerifyCsvWriterProducesStableHeaderAndRow();
    VerifySummaryAccumulatorTracksFinalHashAndPercentiles();
    VerifySummaryCsvWriterProducesStableHeaderAndRow();
    std::cout << "Headless metrics tests passed\n";
    return 0;
}
