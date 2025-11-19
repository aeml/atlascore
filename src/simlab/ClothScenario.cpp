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
            
            physics::EnvironmentForces env;
            env.gravityY = -9.81f;
            env.drag = 0.05f; // Higher drag for cloth to stop it going crazy
            env.windX = 3.0f; // Add some wind to make it flutter!
            
            auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
            physicsSystem->SetEnvironment(env);
            physicsSystem->SetJobSystem(&m_jobSystem);
            world.AddSystem(std::move(physicsSystem));

            // Cloth parameters
            int rows = 10;
            int cols = 10;
            float spacing = 1.0f;
            float startX = 0.0f;
            float startY = 4.0f;

            std::vector<std::vector<ecs::EntityId>> grid(rows, std::vector<ecs::EntityId>(cols));

            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    float x = startX + c * spacing;
                    float y = startY - r * spacing;
                    
                    // Top row is static (pinned)
                    float mass = (r == 0) ? 0.0f : 1.0f;
                    
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
        }

        void Step(ecs::World& world, float dt) override
        {
            m_renderer->Clear();
            
            // Draw joints
            auto* jointStorage = world.GetStorage<physics::DistanceJointComponent>();
            auto* tfStorage = world.GetStorage<physics::TransformComponent>();
            
            if (jointStorage && tfStorage)
            {
                const auto& joints = jointStorage->GetData();
                for (const auto& joint : joints)
                {
                    auto* tA = tfStorage->Get(joint.entityA);
                    auto* tB = tfStorage->Get(joint.entityB);
                    if (tA && tB)
                    {
                        DrawLine(tA->x, tA->y, tB->x, tB->y);
                    }
                }
            }

            // Draw bodies
            world.ForEach<physics::TransformComponent>([&](ecs::EntityId id, physics::TransformComponent& tf) {
                int sx, sy;
                WorldToScreen(tf.x, tf.y, m_width, m_height, m_cfg, sx, sy);
                if (sx >= 0 && sx < m_width && sy >= 0 && sy < m_height)
                {
                    m_renderer->Put(sx, sy, '#');
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

        ecs::EntityId CreateBody(ecs::World& world, float x, float y, float mass)
        {
            auto e = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{x, y});
            float invMass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
            world.AddComponent<physics::RigidBodyComponent>(e, physics::RigidBodyComponent{
                .vx = 0.0f, 
                .vy = 0.0f, 
                .lastX = x, 
                .lastY = y, 
                .mass = mass, 
                .invMass = invMass
            });
            // Small AABB for cloth particles
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

        void DrawLine(float x1, float y1, float x2, float y2)
        {
            int sx1, sy1, sx2, sy2;
            WorldToScreen(x1, y1, m_width, m_height, m_cfg, sx1, sy1);
            WorldToScreen(x2, y2, m_width, m_height, m_cfg, sx2, sy2);
            m_renderer->DrawLine(sx1, sy1, sx2, sy2, '.');
        }
    };

    std::unique_ptr<IScenario> CreateClothScenario()
    {
        return std::make_unique<ClothScenario>();
    }
}
