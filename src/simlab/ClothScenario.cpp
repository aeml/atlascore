#include "simlab/Scenario.hpp"
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"
#include "ascii/TextRenderer.hpp"
#include <vector>
#include <iostream>

namespace simlab
{
    namespace
    {
        struct ClothConfig
        {
            float worldMinX = -5.0f;
            float worldMaxX = 15.0f;
            float worldMinY = -15.0f;
            float worldMaxY = 5.0f;
        };

        void WorldToScreen(float wx, float wy, int sw, int sh, const ClothConfig& cfg, int& sx, int& sy)
        {
            float spanX = cfg.worldMaxX - cfg.worldMinX;
            float spanY = cfg.worldMaxY - cfg.worldMinY;
            
            float normX = (wx - cfg.worldMinX) / spanX;
            float normY = (wy - cfg.worldMinY) / spanY;
            
            sx = static_cast<int>(normX * (sw - 1));
            sy = (sh - 1) - static_cast<int>(normY * (sh - 1));
        }
    }

    class ClothScenario : public IScenario
    {
    public:
        void Setup(ecs::World& world) override
        {
            m_width = 80;
            m_height = 24;
            m_renderer = std::make_unique<ascii::TextRenderer>(m_width, m_height);
            if (simlab::IsHeadlessRendering())
            {
                m_renderer->SetHeadless(true);
            }
            
            physics::EnvironmentForces env;
            env.gravityY = -9.81f;
            env.drag = 0.5f; // High drag for damping
            env.windX = 0.0f; // No wind
            
            std::cout << "ClothScenario Setup: Gravity=" << env.gravityY << " Wind=" << env.windX << std::endl;

            auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
            physics::PhysicsSettings settings;
            settings.substeps = 20;
            settings.positionIterations = 28;
            settings.velocityIterations = 14;
            settings.constraintIterations = 20;
            settings.correctionPercent = 0.25f;
            settings.maxPositionCorrection = 0.08f;
            physicsSystem->SetSettings(settings);
            physicsSystem->SetEnvironment(env);
            physicsSystem->SetJobSystem(&m_jobSystem);
            world.AddSystem(std::move(physicsSystem));

            // Cloth parameters
            int rows = 12;
            int cols = 12;
            float spacing = 0.8f;
            float startX = 0.0f;
            float startY = 4.0f; // Start lower to be visible

            std::vector<std::vector<ecs::EntityId>> grid(rows, std::vector<ecs::EntityId>(cols));

            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    float x = startX + c * spacing;
                    float y = startY - r * spacing;
                    
                    bool pinned = (r == 0);
                    float mass = pinned ? 0.0f : 1.0f;
                    
                    grid[r][c] = CreateBody(world, x, y, mass);
                }
            }

            // Create joints
            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    ecs::EntityId current = grid[r][c];

                    // Horizontal joint (to the right)
                    if (c < cols - 1)
                    {
                        ecs::EntityId right = grid[r][c + 1];
                        CreateJoint(world, current, right, spacing);
                    }

                    // Vertical joint (down)
                    if (r < rows - 1)
                    {
                        ecs::EntityId down = grid[r + 1][c];
                        CreateJoint(world, current, down, spacing);
                    }
                }
            }

            // Positioned to intercept the cloth
            // Cloth center X is startX + (cols-1)*spacing/2 = 0 + 11*0.8/2 = 4.4
            // Let's put obstacle at 4.4
            m_obstacleId = CreateObstacle(world, 4.4f, -2.0f, 2.5f);
        }

        void Step(ecs::World& world, float /*dt*/) override
        {
            m_renderer->Clear(' ', ascii::Color::Default);
            
            // Draw obstacle
            auto* tfStorage = world.GetStorage<physics::TransformComponent>();
            if (tfStorage)
            {
                auto* obsTf = tfStorage->Get(m_obstacleId);
                if (obsTf)
                {
                    int sx, sy;
                    WorldToScreen(obsTf->x, obsTf->y, m_width, m_height, m_cfg, sx, sy);
                    
                    // Calculate screen radii
                    float spanX = m_cfg.worldMaxX - m_cfg.worldMinX;
                    float spanY = m_cfg.worldMaxY - m_cfg.worldMinY;
                    
                    // Radius is 2.5f
                    int rx = static_cast<int>((2.5f / spanX) * m_width);
                    int ry = static_cast<int>((2.5f / spanY) * m_height);
                    
                    m_renderer->FillEllipse(sx, sy, rx, ry, 'O', ascii::Color::Red);
                }
            }

            // Draw joints
            auto* jointStorage = world.GetStorage<physics::DistanceJointComponent>();
            
            if (jointStorage && tfStorage)
            {
                const auto& joints = jointStorage->GetData();
                for (const auto& joint : joints)
                {
                    auto* tA = tfStorage->Get(joint.entityA);
                    auto* tB = tfStorage->Get(joint.entityB);
                    if (tA && tB)
                    {
                        DrawLine(tA->x, tA->y, tB->x, tB->y, ascii::Color::White);
                    }
                }
            }

            // Draw bodies (cloth particles)
            // We iterate all transforms, but we should filter out the obstacle.
            // The obstacle has mass 0, but so do pinned cloth points.
            // We can just draw all bodies as Cyan, and the obstacle (drawn first) might be overwritten or we draw obstacle last.
            // Let's draw obstacle first (above), then joints, then particles.
            // Actually, let's just draw particles that are NOT the obstacle.
            
            world.ForEach<physics::TransformComponent>([&](ecs::EntityId id, physics::TransformComponent& tf) {
                if (id == m_obstacleId) return; // Skip obstacle, handled separately

                int sx, sy;
                WorldToScreen(tf.x, tf.y, m_width, m_height, m_cfg, sx, sy);
                if (sx >= 0 && sx < m_width && sy >= 0 && sy < m_height)
                {
                    m_renderer->Put(sx, sy, '*', ascii::Color::Cyan);
                }
            });

            m_renderer->PresentDiff(std::cout);
        }

    private:
        jobs::JobSystem m_jobSystem;
        std::unique_ptr<ascii::TextRenderer> m_renderer;
        ClothConfig m_cfg;
        int m_width;
        int m_height;
        ecs::EntityId m_obstacleId;

        ecs::EntityId CreateBody(ecs::World& world, float x, float y, float mass)
        {
            auto e = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{x, y, 0.0f});
            physics::RigidBodyComponent rb{};
            rb.vx = 0.0f;
            rb.vy = 0.0f;
            rb.lastX = x;
            rb.lastY = y;
            rb.lastAngle = 0.0f;
            rb.mass = mass;
            rb.invMass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
            rb.restitution = 0.0f;
            rb.friction = 10.0f;
            rb.angularFriction = 1.0f;
            rb.angularDrag = 0.05f;
            physics::ConfigureCircleInertia(rb, 0.1f);
            world.AddComponent<physics::RigidBodyComponent>(e, rb);
            world.AddComponent<physics::AABBComponent>(e, physics::AABBComponent{x - 0.1f, y - 0.1f, x + 0.1f, y + 0.1f});
            return e;
        }

        void CreateJoint(ecs::World& world, ecs::EntityId a, ecs::EntityId b, float dist)
        {
            auto e = world.CreateEntity();
            world.AddComponent<physics::DistanceJointComponent>(e, physics::DistanceJointComponent{
                static_cast<uint32_t>(a), 
                static_cast<uint32_t>(b), 
                dist, 
                0.0f
            });
        }

        ecs::EntityId CreateObstacle(ecs::World& world, float x, float y, float radius)
        {
            auto e = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{x, y, 0.0f});
            physics::RigidBodyComponent rb{};
            rb.vx = 0.0f;
            rb.vy = 0.0f;
            rb.lastX = x;
            rb.lastY = y;
            rb.lastAngle = 0.0f;
            rb.mass = 0.0f;
            rb.invMass = 0.0f;
            rb.restitution = 0.0f;
            rb.friction = 10.0f;
            rb.inertia = 0.0f;
            rb.invInertia = 0.0f;
            world.AddComponent<physics::RigidBodyComponent>(e, rb);
            world.AddComponent<physics::AABBComponent>(e, physics::AABBComponent{x - radius, y - radius, x + radius, y + radius});
            world.AddComponent<physics::CircleColliderComponent>(e, physics::CircleColliderComponent{radius, 0.0f, 0.0f});
            return e;
        }

        void DrawLine(float x1, float y1, float x2, float y2, ascii::Color color)
        {
            int sx1, sy1, sx2, sy2;
            WorldToScreen(x1, y1, m_width, m_height, m_cfg, sx1, sy1);
            WorldToScreen(x2, y2, m_width, m_height, m_cfg, sx2, sy2);
            m_renderer->DrawLine(sx1, sy1, sx2, sy2, '.', color);
        }
    };

    std::unique_ptr<IScenario> CreateClothScenario()
    {
        return std::make_unique<ClothScenario>();
    }
}
