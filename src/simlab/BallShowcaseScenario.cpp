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

#include "simlab/Scenario.hpp"

#include "ascii/TextRenderer.hpp"
#include "ecs/World.hpp"
#include "jobs/JobSystem.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace simlab
{
    namespace
    {
        struct ShowcaseConfig
        {
            float worldMinX = -12.0f;
            float worldMaxX = 12.0f;
            float worldMinY = -6.0f;
            float worldMaxY = 8.0f;
        };

        struct BallDesc
        {
            float x;
            float y;
            float radius;
            float mass;
            float vx;
            float vy;
            float restitution;
            float friction;
        };

        void WorldToScreen(float wx, float wy, const ShowcaseConfig& cfg, int sw, int sh, int& sx, int& sy)
        {
            const float spanX = cfg.worldMaxX - cfg.worldMinX;
            const float spanY = cfg.worldMaxY - cfg.worldMinY;
            const float nx = (wx - cfg.worldMinX) / spanX;
            const float ny = (wy - cfg.worldMinY) / spanY;
            sx = static_cast<int>(nx * (sw - 1));
            sy = (sh - 1) - static_cast<int>(ny * (sh - 1));
        }

        class BallShowcaseScenario final : public IScenario
        {
        public:
            void Setup(ecs::World& world) override
            {
                m_renderer = std::make_unique<ascii::TextRenderer>(m_width, m_height);
                if (IsHeadlessRendering())
                {
                    m_renderer->SetHeadless(false); // we'll control full vs diff manually
                }

                physics::EnvironmentForces env{};
                env.gravityY = -12.0f;
                env.drag = 0.05f;

                auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
                physics::PhysicsSettings settings;
                // Tuned down for stability (iterative refinement): fewer substeps & iterations reduce overshoot.
                settings.substeps = 8;
                settings.positionIterations = 12;
                settings.velocityIterations = 8;
                settings.constraintIterations = 6;
                settings.correctionPercent = 0.15f;
                settings.maxPositionCorrection = 0.02f;
                physicsSystem->SetSettings(settings);
                physicsSystem->SetEnvironment(env);
                physicsSystem->SetJobSystem(&m_jobSystem);
                world.AddSystem(std::move(physicsSystem));

                BuildScenes(world);
            }

            void Update(ecs::World& /*world*/, float /*dt*/) override
            {
                // Physics handled by systems
            }

            void Render(ecs::World& world, std::ostream& out) override
            {
                if (!m_renderer)
                {
                    return;
                }

                m_renderer->Clear(' ');
                DrawGrid();

                world.ForEach<physics::CircleColliderComponent>([&](ecs::EntityId id, physics::CircleColliderComponent& circle)
                {
                    auto* tf = world.GetComponent<physics::TransformComponent>(id);
                    if (!tf)
                    {
                        return;
                    }
                    DrawCircle(*tf, circle.radius);
                });

                world.ForEach<physics::AABBComponent>([&](ecs::EntityId id, physics::AABBComponent& box)
                {
                    auto* rb = world.GetComponent<physics::RigidBodyComponent>(id);
                    if (rb && rb->mass == 0.0f)
                    {
                        DrawPlatform(box);
                    }
                });

                // Every k frames, emit a full frame snapshot; otherwise emit diff for compaction.
                if (IsHeadlessRendering())
                {
                    // Clamp excessive velocities for stability (scenario-local safeguard).
                    const float maxSpeed = 15.0f;
                    world.ForEach<physics::RigidBodyComponent>([&](ecs::EntityId id, physics::RigidBodyComponent& rb)
                    {
                        if (rb.mass > 0.0f)
                        {
                            float speed = std::sqrt(rb.vx * rb.vx + rb.vy * rb.vy);
                            if (speed > maxSpeed && speed > 0.0f)
                            {
                                float scale = maxSpeed / speed;
                                rb.vx *= scale;
                                rb.vy *= scale;
                            }
                        }
                    });
                    // Rendering order fix: draw platforms first, then circles (balls) so circles appear on top.
                    // Rebuild the frame with proper layering.
                    m_renderer->Clear(' ');
                    DrawGrid();
                    // Draw platforms (static AABBs) first
                    world.ForEach<physics::AABBComponent>([&](ecs::EntityId id, physics::AABBComponent& box)
                    {
                        auto* rb = world.GetComponent<physics::RigidBodyComponent>(id);
                        if (rb && rb->mass == 0.0f)
                        {
                            DrawPlatform(box);
                        }
                    });
                    // Then dynamic circles
                    world.ForEach<physics::CircleColliderComponent>([&](ecs::EntityId id, physics::CircleColliderComponent& circle)
                    {
                        auto* tf = world.GetComponent<physics::TransformComponent>(id);
                        if (!tf) return;
                        DrawCircle(*tf, circle.radius);
                    });

                    if ((m_frameCounter % m_fullFrameInterval) == 0)
                    {
                        out << "--- FRAME " << m_frameCounter << " START ---\n";
                        // Diagnostic: dump ball positions & velocities for analysis.
                        for (size_t i = 0; i < m_balls.size(); ++i)
                        {
                            auto id = m_balls[i];
                            auto* tf = world.GetComponent<physics::TransformComponent>(id);
                            auto* rb = world.GetComponent<physics::RigidBodyComponent>(id);
                            if (tf && rb)
                            {
                                out << "#BALL " << i << " id=" << id << " x=" << tf->x << " y=" << tf->y
                                    << " vx=" << rb->vx << " vy=" << rb->vy << " m=" << rb->mass << "\n";
                            }
                        }
                        m_renderer->PresentFull(out); // includes its own START/END markers; custom header assists indexing
                    }
                    else
                    {
                        m_renderer->PresentDiff(out);
                    }
                    ++m_frameCounter;
                }
                else
                {
                    m_renderer->PresentDiff(out);
                }
            }

        private:
            void BuildScenes(ecs::World& world)
            {
                // Full viewport boundary: left, right (existing), plus top & bottom so balls remain inside.
                // Left and right vertical walls.
                CreateBoundary(world, m_cfg.worldMinX + 0.2f, m_cfg.worldMinY, m_cfg.worldMinX + 0.6f, m_cfg.worldMaxY);
                CreateBoundary(world, m_cfg.worldMaxX - 0.6f, m_cfg.worldMinY, m_cfg.worldMaxX - 0.2f, m_cfg.worldMaxY);
                // Bottom floor (thin horizontal strip).
                CreateBoundary(world, m_cfg.worldMinX, m_cfg.worldMinY, m_cfg.worldMaxX, m_cfg.worldMinY + 0.4f);
                // Top ceiling.
                CreateBoundary(world, m_cfg.worldMinX, m_cfg.worldMaxY - 0.4f, m_cfg.worldMaxX, m_cfg.worldMaxY);

                // Two platforms only: one higher on the left, one lower on the right; empty space in the middle.
                const float leftPlatformCenterX = -8.0f;
                const float leftPlatformCenterY = 2.0f;   // higher platform (adjusted for visual alignment)
                const float leftPlatformWidth   = 5.5f;
                const float leftPlatformHeight  = 0.8f;
                CreateStaticPlatform(world, leftPlatformCenterX, leftPlatformCenterY, leftPlatformWidth, leftPlatformHeight);

                const float rightPlatformCenterX = 8.0f;
                const float rightPlatformCenterY = -2.8f; // lower platform
                const float rightPlatformWidth   = 5.5f;
                const float rightPlatformHeight  = 0.8f;
                CreateStaticPlatform(world, rightPlatformCenterX, rightPlatformCenterY, rightPlatformWidth, rightPlatformHeight);

                // Spawn balls above platforms & in middle so they either land on a platform or the bottom floor.
                // Ball radius kept moderate; minimal initial horizontal velocity for unbiased falling.
                const float r = 0.6f;
                auto spawnAbove = [&](float cx, float cy, float extraHeight, float mass, float vx, float vy, float restitution)
                {
                    float top = cy + ( (cx == leftPlatformCenterX) ? leftPlatformHeight * 0.5f : rightPlatformHeight * 0.5f );
                    float y = top + r + extraHeight;
                    CreateBall(world, BallDesc{cx, y, r, mass, vx, vy, restitution, 0.25f});
                };

                // Left platform cluster (higher) - slight horizontal spread.
                spawnAbove(leftPlatformCenterX - 1.2f, leftPlatformCenterY, 0.2f, 1.0f, 0.15f, 0.0f, 0.35f);
                spawnAbove(leftPlatformCenterX,          leftPlatformCenterY, 0.4f, 1.1f, 0.0f, 0.0f, 0.35f);
                spawnAbove(leftPlatformCenterX + 1.2f, leftPlatformCenterY, 0.25f, 0.9f, -0.15f, 0.0f, 0.35f);

                // Middle free-fall balls (will land on bottom).
                CreateBall(world, BallDesc{-1.5f, 7.0f, r, 1.0f, 0.05f, -0.2f, 0.38f, 0.25f});
                CreateBall(world, BallDesc{0.0f,  7.6f, r, 1.2f, 0.0f,  -0.3f, 0.40f, 0.25f});
                CreateBall(world, BallDesc{1.5f,  7.3f, r, 0.95f,-0.05f,-0.2f, 0.38f, 0.25f});

                // Right platform cluster (lower) - spawn high up to fall onto platform.
                CreateBall(world, BallDesc{rightPlatformCenterX - 1.4f, 7.0f, r, 1.05f, 0.12f, 0.0f, 0.32f, 0.25f});
                CreateBall(world, BallDesc{rightPlatformCenterX,        7.6f, r, 1.15f, 0.0f,  0.0f, 0.32f, 0.25f});
                CreateBall(world, BallDesc{rightPlatformCenterX + 1.4f, 7.3f, r, 0.9f, -0.12f, 0.0f, 0.32f, 0.25f});
            }

            void CreateBoundary(ecs::World& world, float minX, float minY, float maxX, float maxY)
            {
                auto entity = world.CreateEntity();
                physics::TransformComponent tf{};
                tf.x = (minX + maxX) * 0.5f;
                tf.y = (minY + maxY) * 0.5f;
                tf.rotation = 0.0f;
                world.AddComponent<physics::TransformComponent>(entity, tf);

                physics::RigidBodyComponent rb{};
                rb.mass = 0.0f;
                rb.invMass = 0.0f;
                rb.inertia = 0.0f;
                rb.invInertia = 0.0f;
                rb.friction = 0.9f;
                rb.restitution = 0.05f;
                rb.lastX = tf.x;
                rb.lastY = tf.y;
                rb.lastAngle = 0.0f;
                world.AddComponent<physics::RigidBodyComponent>(entity, rb);

                physics::AABBComponent box{};
                box.minX = minX;
                box.maxX = maxX;
                box.minY = minY;
                box.maxY = maxY;
                world.AddComponent<physics::AABBComponent>(entity, box);
            }

            void CreateStaticPlatform(ecs::World& world, float centerX, float centerY, float width, float height)
            {
                auto entity = world.CreateEntity();
                physics::TransformComponent tf{};
                tf.x = centerX;
                tf.y = centerY;
                tf.rotation = 0.0f;
                world.AddComponent<physics::TransformComponent>(entity, tf);

                physics::RigidBodyComponent rb{};
                rb.mass = 0.0f;
                rb.invMass = 0.0f;
                rb.inertia = 0.0f;
                rb.invInertia = 0.0f;
                rb.friction = 0.9f;
                rb.restitution = 0.1f;
                rb.lastX = centerX;
                rb.lastY = centerY;
                rb.lastAngle = 0.0f;
                world.AddComponent<physics::RigidBodyComponent>(entity, rb);

                physics::AABBComponent box{};
                box.minX = centerX - width * 0.5f;
                box.maxX = centerX + width * 0.5f;
                box.minY = centerY - height * 0.5f;
                box.maxY = centerY + height * 0.5f;
                world.AddComponent<physics::AABBComponent>(entity, box);
            }

            ecs::EntityId CreateBall(ecs::World& world, const BallDesc& desc)
            {
                auto entity = world.CreateEntity();
                physics::TransformComponent tf{};
                tf.x = desc.x;
                tf.y = desc.y;
                tf.rotation = 0.0f;
                world.AddComponent<physics::TransformComponent>(entity, tf);

                physics::RigidBodyComponent rb{};
                rb.vx = desc.vx;
                rb.vy = desc.vy;
                rb.lastX = desc.x;
                rb.lastY = desc.y;
                rb.lastAngle = 0.0f;
                rb.mass = desc.mass;
                rb.invMass = desc.mass > 0.0f ? 1.0f / desc.mass : 0.0f;
                rb.restitution = desc.restitution;
                rb.friction = desc.friction;
                rb.angularFriction = 0.25f;
                rb.angularDrag = 0.02f;
                physics::ConfigureCircleInertia(rb, desc.radius);
                world.AddComponent<physics::RigidBodyComponent>(entity, rb);

                physics::AABBComponent bounds{};
                bounds.minX = desc.x - desc.radius;
                bounds.maxX = desc.x + desc.radius;
                bounds.minY = desc.y - desc.radius;
                bounds.maxY = desc.y + desc.radius;
                world.AddComponent<physics::AABBComponent>(entity, bounds);

                physics::CircleColliderComponent circle{};
                circle.radius = desc.radius;
                world.AddComponent<physics::CircleColliderComponent>(entity, circle);
                m_balls.push_back(entity);
                return entity;
            }

            void DrawGrid()
            {
                if (!m_renderer)
                {
                    return;
                }

                const float cellSize = 2.0f;
                float startX = std::floor(m_cfg.worldMinX / cellSize) * cellSize;
                for (float x = startX; x <= m_cfg.worldMaxX; x += cellSize)
                {
                    int sx, sy1, sy2;
                    int dummy;
                    WorldToScreen(x, m_cfg.worldMinY, m_cfg, m_width, m_height, sx, sy1);
                    WorldToScreen(x, m_cfg.worldMaxY, m_cfg, m_width, m_height, dummy, sy2);
                    if (sx >= 0 && sx < m_width)
                    {
                        m_renderer->DrawLine(sx, std::max(0, sy2), sx, std::min(m_height - 1, sy1), '.');
                    }
                }

                float startY = std::floor(m_cfg.worldMinY / cellSize) * cellSize;
                for (float y = startY; y <= m_cfg.worldMaxY; y += cellSize)
                {
                    int sy, sx1, sx2;
                    int dummy;
                    WorldToScreen(m_cfg.worldMinX, y, m_cfg, m_width, m_height, sx1, sy);
                    WorldToScreen(m_cfg.worldMaxX, y, m_cfg, m_width, m_height, sx2, dummy);
                    if (sy >= 0 && sy < m_height)
                    {
                        m_renderer->DrawLine(std::max(0, sx1), sy, std::min(m_width - 1, sx2), sy, '.');
                    }
                }
            }

            void DrawCircle(const physics::TransformComponent& tf, float radius)
            {
                int sx, sy;
                WorldToScreen(tf.x, tf.y, m_cfg, m_width, m_height, sx, sy);

                const float spanX = m_cfg.worldMaxX - m_cfg.worldMinX;
                const float spanY = m_cfg.worldMaxY - m_cfg.worldMinY;
                int rx = std::max(1, static_cast<int>((radius / spanX) * m_width));
                int ry = std::max(1, static_cast<int>((radius / spanY) * m_height));

                m_renderer->FillEllipse(sx, sy, rx, ry, 'o', ascii::Color::Yellow);
            }

            void DrawPlatform(const physics::AABBComponent& box)
            {
                int x1, y1, x2, y2;
                WorldToScreen(box.minX, box.maxY, m_cfg, m_width, m_height, x1, y1);
                WorldToScreen(box.maxX, box.minY, m_cfg, m_width, m_height, x2, y2);
                x1 = std::max(0, std::min(x1, m_width - 1));
                x2 = std::max(0, std::min(x2, m_width - 1));
                y1 = std::max(0, std::min(y1, m_height - 1));
                y2 = std::max(0, std::min(y2, m_height - 1));
                for (int y = y1; y <= y2; ++y)
                {
                    for (int x = x1; x <= x2; ++x)
                    {
                        m_renderer->Put(x, y, '=', ascii::Color::Green);
                    }
                }
            }

            jobs::JobSystem m_jobSystem;
            std::unique_ptr<ascii::TextRenderer> m_renderer;
            ShowcaseConfig m_cfg;
            int m_width{90};
            int m_height{28};
            int m_frameCounter{0};
            int m_fullFrameInterval{15};
            std::vector<ecs::EntityId> m_balls; // instrumented list of dynamic balls
        };
    }

    std::unique_ptr<IScenario> CreateBallShowcaseScenario()
    {
        return std::make_unique<BallShowcaseScenario>();
    }

    static ScenarioAutoRegister g_reg_ball_showcase{
        "ball_showcase",
        "Ball collision showcase",
        &CreateBallShowcaseScenario,
        "Physics Demos",
        "Rigid Bodies"};
}
