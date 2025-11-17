#pragma once

#include "ecs/World.hpp"
#include "physics/Components.hpp"

#include <vector>

namespace physics
{
    // Integrates rigid bodies into transforms applying gravity.
    class PhysicsIntegrationSystem : public ecs::ISystem
    {
    public:
        void Update(ecs::World& world, float dt) override; // Placeholder for future ECS usage.

        void Integrate(std::vector<TransformComponent>& transforms,
                       std::vector<RigidBodyComponent>& bodies,
                       float                            dt) const;

        float Gravity() const noexcept { return m_gravity; }

    private:
        float m_gravity{-9.81f};
    };
}
