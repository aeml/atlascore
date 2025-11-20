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
                if (simlab::IsHeadlessRendering())
                {
                    m_renderer->SetHeadless(true);
                }

                physics::EnvironmentForces env{};
                env.gravityY = -12.0f;
                env.drag = 0.05f;

                auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
                physics::PhysicsSettings settings;
                settings.substeps = 20;
                settings.positionIterations = 28;
                settings.velocityIterations = 16;
                settings.constraintIterations = 12;
                settings.correctionPercent = 0.25f;
                settings.maxPositionCorrection = 0.08f;
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

            void Render(ecs::World& world) override
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

                m_renderer->PresentDiff(std::cout);
            }

        private:
            void BuildScenes(ecs::World& world)
            {
                // Contain the action so balls can't leave the screen instantly.
                CreateBoundary(world, m_cfg.worldMinX + 0.2f, m_cfg.worldMinY, m_cfg.worldMinX + 0.6f, m_cfg.worldMaxY);
                CreateBoundary(world, m_cfg.worldMaxX - 0.6f, m_cfg.worldMinY, m_cfg.worldMaxX - 0.2f, m_cfg.worldMaxY);

                // Lane 1: horizontal hit (left side) with moderate impact
                CreateStaticPlatform(world, -8.0f, -3.0f, 6.0f, 0.5f);
                CreateBall(world, BallDesc{-10.2f, -1.7f, 0.65f, 1.3f, 3.8f, -0.1f, 0.7f, 0.3f});
                CreateBall(world, BallDesc{-7.0f, -1.7f, 0.65f, 1.2f, 0.0f, 0.0f, 0.6f, 0.35f});

                // Lane 2: dropping hammer on cluster (center)
                CreateStaticPlatform(world, -0.5f, -3.6f, 6.0f, 0.8f);
                const float baseY = -2.9f;
                CreateBall(world, BallDesc{-1.0f, baseY, 0.55f, 1.0f, 0.0f, 0.0f, 0.35f, 0.65f});
                CreateBall(world, BallDesc{0.0f, baseY, 0.55f, 1.0f, 0.0f, 0.0f, 0.35f, 0.65f});
                CreateBall(world, BallDesc{1.0f, baseY, 0.55f, 1.0f, 0.0f, 0.0f, 0.35f, 0.65f});
                CreateBall(world, BallDesc{-0.5f, baseY + 0.8f, 0.55f, 1.0f, 0.0f, 0.0f, 0.35f, 0.65f});
                CreateBall(world, BallDesc{0.5f, baseY + 0.8f, 0.55f, 1.0f, 0.0f, 0.0f, 0.35f, 0.65f});
                CreateBall(world, BallDesc{0.0f, 1.2f, 0.7f, 2.5f, 0.0f, -1.5f, 0.5f, 0.25f});

                // Lane 3: glancing strike (right side) with downward entry to keep it on screen
                CreateStaticPlatform(world, 8.0f, -1.0f, 5.0f, 0.4f);
                CreateBall(world, BallDesc{7.5f, 0.0f, 0.6f, 1.2f, 0.0f, 0.0f, 0.7f, 0.2f});
                CreateBall(world, BallDesc{9.0f, 0.7f, 0.6f, 1.2f, 0.0f, 0.0f, 0.7f, 0.2f});
                CreateBall(world, BallDesc{6.0f, 0.4f, 0.55f, 1.0f, 4.2f, -0.35f, 0.55f, 0.2f});
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
