#pragma once

#include "ecs/World.hpp"
#include "physics/Components.hpp"

namespace physics
{
    class PhysicsIntegrationSystem : public ecs::ISystem
    {
    public:
        void Update(ecs::World& world, float dt) override;
    };
}
