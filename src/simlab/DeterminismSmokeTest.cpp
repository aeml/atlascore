#include "simlab/Scenario.hpp"

#include "core/Logger.hpp"
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "physics/EcsSystems.hpp"
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
                m_logger.Info("[simlab] Setup determinism smoke test (ECS falling bodies)");
                // Add systems
                world.AddSystem(std::make_unique<physics::PhysicsEcsSystem>());
                world.AddSystem(std::make_unique<physics::CollisionEcsSystem>());
                // Create a few demo entities with components
                const int bodyCount = 3;
                for (int i = 0; i < bodyCount; ++i)
                {
                    auto e = world.CreateEntity();
                    world.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{static_cast<float>(i*2), 10.0f + static_cast<float>(i*5)});
                    world.AddComponent<physics::RigidBodyComponent>(e, physics::RigidBodyComponent{0.0f, 0.0f});
                    // 1x1 box centered on position
                    auto* t = world.GetComponent<physics::TransformComponent>(e);
                    physics::AABBComponent box{t->x - 0.5f, t->y - 0.5f, t->x + 0.5f, t->y + 0.5f};
                    world.AddComponent<physics::AABBComponent>(e, box);
                }
            }

            void Step(ecs::World& world, float dt) override
            {
                (void)dt;
                (void)world;
                // Rendering skipped here; systems run in world.Update from main loop
            }

        private:
            core::Logger                             m_logger;
            physics::PhysicsIntegrationSystem        m_physics; // legacy (unused)
            ascii::Renderer                          m_renderer{std::cout}; // legacy (unused)
        };
    }

    std::unique_ptr<IScenario> CreateDeterminismSmokeTest()
    {
        return std::make_unique<DeterminismSmokeTest>();
    }

    // Auto-register
    static ScenarioAutoRegister g_reg_smoke{"smoke", "ECS falling bodies smoke test", &CreateDeterminismSmokeTest};
}
