#include "simlab/Scenario.hpp"

#include "core/Logger.hpp"
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "ascii/Renderer.hpp"

#include <vector>
#include <iostream>

namespace simlab
{
    namespace
    {
        class DeterminismSmokeTest final : public IScenario
        {
        public:
            void Setup(ecs::World& world) override
            {
                (void)world;
                m_logger.Info("[simlab] Setup determinism smoke test (falling bodies demo)");

                // Create a few demo bodies.
                const int bodyCount = 3;
                m_transforms.reserve(bodyCount);
                m_bodies.reserve(bodyCount);

                for (int i = 0; i < bodyCount; ++i)
                {
                    physics::TransformComponent t;
                    t.x = static_cast<float>(i * 2);
                    t.y = 10.0f + static_cast<float>(i * 5);
                    physics::RigidBodyComponent b;
                    b.vx = 0.0f;
                    b.vy = 0.0f;
                    m_transforms.push_back(t);
                    m_bodies.push_back(b);
                }
            }

            void Step(ecs::World& world, float dt) override
            {
                (void)world;
                m_physics.Integrate(m_transforms, m_bodies, dt);
                m_renderer.RenderBodies(m_transforms, m_bodies);
            }

        private:
            core::Logger                             m_logger;
            physics::PhysicsIntegrationSystem        m_physics;
            ascii::Renderer                          m_renderer{std::cout};
            std::vector<physics::TransformComponent> m_transforms;
            std::vector<physics::RigidBodyComponent> m_bodies;
        };
    }

    std::unique_ptr<IScenario> CreateDeterminismSmokeTest()
    {
        return std::make_unique<DeterminismSmokeTest>();
    }
}
