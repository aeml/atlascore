#pragma once

#include "ecs/World.hpp"
#include "physics/Components.hpp"

#include <vector>

namespace jobs { class JobSystem; }

namespace physics
{
    // Integrates rigid bodies into transforms applying gravity / environment forces.
    class PhysicsIntegrationSystem : public ecs::ISystem
    {
    public:
        void Update(ecs::World& world, float dt) override; // Placeholder for future ECS usage.

        void Integrate(std::vector<TransformComponent>& transforms,
                       std::vector<RigidBodyComponent>& bodies,
                       float                            dt) const;

        float Gravity() const noexcept { return m_env.gravityY; }

        void SetEnvironment(const EnvironmentForces& env) { m_env = env; }
        const EnvironmentForces& Environment() const noexcept { return m_env; }

        void SetJobSystem(jobs::JobSystem* jobSystem) { m_jobSystem = jobSystem; }

    private:
        EnvironmentForces m_env{};
        jobs::JobSystem* m_jobSystem{nullptr};
    };
}
