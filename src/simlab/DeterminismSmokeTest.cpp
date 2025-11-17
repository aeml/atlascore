#include "simlab/Scenario.hpp"

#include "core/Logger.hpp"
#include "ecs/World.hpp"

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
                m_logger.Info("[simlab] Setup determinism smoke test");
            }

            void Step(ecs::World& world, float dt) override
            {
                (void)world;
                (void)dt;
                // Intentionally minimal; will later hash world state.
            }

        private:
            core::Logger m_logger;
        };
    }

    std::unique_ptr<IScenario> CreateDeterminismSmokeTest()
    {
        return std::make_unique<DeterminismSmokeTest>();
    }
}
