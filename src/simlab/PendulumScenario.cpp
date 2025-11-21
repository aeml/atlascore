#include "simlab/Scenario.hpp"
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"
#include "ascii/TextRenderer.hpp"
#include "core/Logger.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace simlab
{
    namespace
    {
        struct PendulumConfig
        {
            float worldMinX = -10.0f;
            float worldMaxX = 10.0f;
            float worldMinY = -10.0f;
            float worldMaxY = 5.0f;
        };

        void WorldToScreen(float wx, float wy, int sw, int sh, const PendulumConfig& cfg, int& sx, int& sy)
        {
            float spanX = cfg.worldMaxX - cfg.worldMinX;
            float spanY = cfg.worldMaxY - cfg.worldMinY;
            
            float normX = (wx - cfg.worldMinX) / spanX;
            float normY = (wy - cfg.worldMinY) / spanY;
            
            sx = static_cast<int>(normX * (sw - 1));
            sy = (sh - 1) - static_cast<int>(normY * (sh - 1));
        }
    }

    class PendulumScenario : public IScenario
    {
    public:
        void Setup(ecs::World& world) override
        {
            m_width = 80;
            m_height = 24;
            m_renderer = std::make_unique<ascii::TextRenderer>(m_width, m_height);
            
            physics::EnvironmentForces env;
            env.gravityY = -9.81f;
            env.drag = 0.01f; 
            
            auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
            physicsSystem->SetEnvironment(env);
            physicsSystem->SetJobSystem(&m_jobSystem);
            world.AddSystem(std::move(physicsSystem));

            // Anchor point (Static)
            auto anchor = CreateBody(world, 0.0f, 4.0f, 0.0f); 

            // Chain
            int links = 5;
            float linkLength = 1.5f;
            ecs::EntityId prev = anchor;
            
            for (int i = 0; i < links; ++i)
            {
                // Start horizontally to make it swing
                float x = (i + 1) * linkLength;
                float y = 4.0f;
                
                auto next = CreateBody(world, x, y, 1.0f);
                
                // Add joint
                auto jointEntity = world.CreateEntity();
                world.AddComponent<physics::DistanceJointComponent>(jointEntity, physics::DistanceJointComponent{prev, next, linkLength, 0.0f});
                
                prev = next;
            }
        }

        void Update(ecs::World& /*world*/, float /*dt*/) override
        {
            // Physics is handled by systems in world.Update()
        }

        void Render(ecs::World& world, std::ostream& out) override
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
            world.ForEach<physics::TransformComponent>([&](ecs::EntityId /*id*/, physics::TransformComponent& tf) {
                int sx, sy;
                WorldToScreen(tf.x, tf.y, m_width, m_height, m_cfg, sx, sy);
                if (sx >= 0 && sx < m_width && sy >= 0 && sy < m_height)
                {
                    m_renderer->Put(sx, sy, 'O');
                }
            });

            m_renderer->PresentDiff(out);
        }

    private:
        jobs::JobSystem m_jobSystem;
        std::unique_ptr<ascii::TextRenderer> m_renderer;
        PendulumConfig m_cfg;
        int m_width;
        int m_height;

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
            rb.restitution = 0.05f;
            rb.friction = 0.8f;
            rb.angularFriction = 0.3f;
            rb.angularDrag = 0.01f;
            physics::ConfigureBoxInertia(rb, 0.4f, 0.4f);
            world.AddComponent<physics::RigidBodyComponent>(e, rb);
            world.AddComponent<physics::AABBComponent>(e, physics::AABBComponent{x - 0.2f, y - 0.2f, x + 0.2f, y + 0.2f});
            return e;
        }

        void DrawLine(float x1, float y1, float x2, float y2)
        {
            int sx1, sy1, sx2, sy2;
            WorldToScreen(x1, y1, m_width, m_height, m_cfg, sx1, sy1);
            WorldToScreen(x2, y2, m_width, m_height, m_cfg, sx2, sy2);
            m_renderer->DrawLine(sx1, sy1, sx2, sy2, '.');
        }
    };

    std::unique_ptr<IScenario> CreatePendulumScenario()
    {
        return std::make_unique<PendulumScenario>();
    }
}
