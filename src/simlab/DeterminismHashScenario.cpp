#include "simlab/Scenario.hpp"
#include "simlab/WorldHasher.hpp"

#include "core/Logger.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"

#include <vector>
#include <fstream>

namespace simlab
{
    namespace
    {
        class DeterminismHashScenario final : public IScenario
        {
        public:
            void Setup(ecs::World& world) override
            {
                (void)world;
                auto logFile = std::make_shared<std::ofstream>("simlab_hash.log", std::ios::app);
                if (logFile->is_open())
                {
                    m_logger.SetOutput(logFile);
                }

                m_logger.Info("[simlab] Setup determinism hash scenario");
                InitBodies(m_transformsA, m_bodiesA);
                InitBodies(m_transformsB, m_bodiesB);
            }

            void Step(ecs::World& world, float dt) override
            {
                (void)world;
                m_physics.Integrate(m_transformsA, m_bodiesA, dt);
                m_physics.Integrate(m_transformsB, m_bodiesB, dt);

                const auto hA = m_hasher.HashBodies(m_transformsA, m_bodiesA);
                const auto hB = m_hasher.HashBodies(m_transformsB, m_bodiesB);

                if (hA != hB)
                {
                    m_logger.Error("Determinism mismatch: hashes differ");
                }
                else
                {
                    m_logger.Info("[determinism] step hash=" + std::to_string(hA));
                }
            }

        private:
            void InitBodies(std::vector<physics::TransformComponent>& t,
                            std::vector<physics::RigidBodyComponent>& b)
            {
                const int bodyCount = 3;
                t.clear();
                b.clear();
                for (int i = 0; i < bodyCount; ++i)
                {
                    physics::TransformComponent tr;
                    tr.x = static_cast<float>(i * 3);
                    tr.y = 20.0f + static_cast<float>(i * 2);
                    physics::RigidBodyComponent rb;
                    rb.vx = 0.0f;
                    rb.vy = 0.0f;
                    t.push_back(tr);
                    b.push_back(rb);
                }
            }

            core::Logger                             m_logger;
            physics::PhysicsIntegrationSystem        m_physics;
            WorldHasher                              m_hasher;
            std::vector<physics::TransformComponent> m_transformsA;
            std::vector<physics::RigidBodyComponent> m_bodiesA;
            std::vector<physics::TransformComponent> m_transformsB;
            std::vector<physics::RigidBodyComponent> m_bodiesB;
        };
    }

    std::unique_ptr<IScenario> CreateDeterminismHashScenario()
    {
        return std::make_unique<DeterminismHashScenario>();
    }

    // Auto-register
    static ScenarioAutoRegister g_reg_hash{"hash", "Determinism hash dual-run scenario", &CreateDeterminismHashScenario};
}
