#include "simlab/Scenario.hpp"
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"
#include "ascii/TextRenderer.hpp"
#include "core/Logger.hpp"
#include <vector>
#include <iostream>
#include <random>

namespace simlab
{
    namespace
    {
        struct StressTestConfig
        {
            float worldMinX = -50.0f;
            float worldMaxX = 50.0f;
            float worldMinY = -50.0f;
            float worldMaxY = 50.0f;
        };

        void WorldToScreen(float wx, float wy, int sw, int sh, const StressTestConfig& cfg, int& sx, int& sy)
        {
            float spanX = cfg.worldMaxX - cfg.worldMinX;
            float spanY = cfg.worldMaxY - cfg.worldMinY;
            
            float normX = (wx - cfg.worldMinX) / spanX;
            float normY = (wy - cfg.worldMinY) / spanY;
            
            sx = static_cast<int>(normX * (sw - 1));
            sy = (sh - 1) - static_cast<int>(normY * (sh - 1));
        }
    }

    class StressTestScenario : public IScenario
    {
    public:
        void Setup(ecs::World& world) override
        {
            m_width = 120;
            m_height = 40;
            m_renderer = std::make_unique<ascii::TextRenderer>(m_width, m_height);
            if (simlab::IsHeadlessRendering())
            {
                m_renderer->SetHeadless(true);
            }
            
            physics::EnvironmentForces env;
            env.gravityY = -9.81f;
            env.drag = 0.01f;
            
            auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
            physicsSystem->SetEnvironment(env);
            physicsSystem->SetJobSystem(&m_jobSystem);
            world.AddSystem(std::move(physicsSystem));

            // Spawn thousands of particles
            std::mt19937 rng(42);
            std::uniform_real_distribution<float> distPos(-40.0f, 40.0f);
            std::uniform_real_distribution<float> distVel(-10.0f, 10.0f);

            const int particleCount = 2000;
            m_logger.Info("Spawning " + std::to_string(particleCount) + " particles for stress test...");

            for (int i = 0; i < particleCount; ++i)
            {
                auto id = world.CreateEntity();
                auto& t = world.AddComponent<physics::TransformComponent>(id);
                t.x = distPos(rng);
                t.y = distPos(rng);
                
                auto& rb = world.AddComponent<physics::RigidBodyComponent>(id);
                rb.mass = 1.0f;
                rb.vx = distVel(rng);
                rb.vy = distVel(rng);

                auto& aabb = world.AddComponent<physics::AABBComponent>(id);
                aabb.minX = t.x - 0.5f;
                aabb.maxX = t.x + 0.5f;
                aabb.minY = t.y - 0.5f;
                aabb.maxY = t.y + 0.5f;
            }
        }

        void Update(ecs::World& world, float dt) override
        {
            // Keep particles in bounds
            world.ForEach<physics::TransformComponent>([&](ecs::EntityId, physics::TransformComponent& t) {
                if (t.x < m_config.worldMinX) t.x = m_config.worldMaxX;
                if (t.x > m_config.worldMaxX) t.x = m_config.worldMinX;
                if (t.y < m_config.worldMinY) t.y = m_config.worldMaxY;
                if (t.y > m_config.worldMaxY) t.y = m_config.worldMinY;
            });
        }

        void Render(ecs::World& world, std::ostream& out) override
        {
            m_renderer->Clear(' ');

            // Draw particles
            world.ForEach<physics::TransformComponent>([&](ecs::EntityId, physics::TransformComponent& t) {
                int x, y;
                WorldToScreen(t.x, t.y, m_width, m_height, m_config, x, y);
                m_renderer->Put(x, y, '.', ascii::Color::Green);
            });

            m_renderer->PresentDiff(out);
        }

    private:
        core::Logger m_logger;
        jobs::JobSystem m_jobSystem;
        std::unique_ptr<ascii::TextRenderer> m_renderer;
        int m_width;
        int m_height;
        StressTestConfig m_config;
    };

    std::unique_ptr<IScenario> CreateStressTestScenario()
    {
        return std::make_unique<StressTestScenario>();
    }

    static ScenarioAutoRegister g_reg_stress{"stress", "Stress Test (2000 particles)", &CreateStressTestScenario, "Benchmarks", "Physics"};
}
