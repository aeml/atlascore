#pragma once

#include <memory>

namespace ecs { class World; }

namespace simlab
{
    class IScenario
    {
    public:
        virtual ~IScenario() = default;
        virtual void Setup(ecs::World& world) = 0;
        virtual void Step(ecs::World& world, float dt) = 0;
    };

    std::unique_ptr<IScenario> CreateDeterminismSmokeTest();
}
