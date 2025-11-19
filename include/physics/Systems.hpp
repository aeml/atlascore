#pragma once

#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/CollisionSystem.hpp"

#include <vector>

namespace jobs { class JobSystem; }

namespace physics
{
    // Resolves collisions by applying impulses.
    class CollisionResolutionSystem
    {
    public:
        // Resolve collisions for raw vectors (legacy/test usage)
        void Resolve(const std::vector<CollisionEvent>& events,
                     std::vector<TransformComponent>& transforms,
                     std::vector<RigidBodyComponent>& bodies) const;

        // Resolve collisions for ECS world
        void Resolve(const std::vector<CollisionEvent>& events, ecs::World& world) const;
    };

    // Resolves constraints (joints).
    class ConstraintResolutionSystem
    {
    public:
        void Resolve(ecs::World& world) const;
    };

    // Integrates rigid bodies into transforms applying gravity / environment forces.
    class PhysicsIntegrationSystem : public ecs::ISystem
    {
    public:
        void Update(ecs::World& world, float dt) override; // Placeholder for future ECS usage.
        void UpdateVelocities(ecs::World& world, float dt);

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

    // Orchestrates the entire physics pipeline: Integration -> Detection -> Resolution
    class PhysicsSystem : public ecs::ISystem
    {
    public:
        void Update(ecs::World& world, float dt) override;

        void SetEnvironment(const EnvironmentForces& env) { m_integration.SetEnvironment(env); }
        void SetJobSystem(jobs::JobSystem* js) { m_jobSystem = js; m_integration.SetJobSystem(js); }

    private:
        PhysicsIntegrationSystem  m_integration;
        CollisionSystem           m_collision;
        CollisionResolutionSystem m_resolution;
        ConstraintResolutionSystem m_constraints;
        std::vector<CollisionEvent> m_events;
        jobs::JobSystem*          m_jobSystem{nullptr};
    };
}
