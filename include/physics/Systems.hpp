#pragma once

#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/CollisionSystem.hpp"

#include <algorithm>
#include <vector>

namespace jobs { class JobSystem; }

namespace physics
{
    struct PhysicsSettings
    {
        int   substeps{16};
        int   positionIterations{20};
        int   velocityIterations{10};
        int   constraintIterations{8};
        float penetrationSlop{0.01f};
        float correctionPercent{0.2f};
        float maxPositionCorrection{0.2f};
    };

    // Resolves collisions by applying impulses.
    class CollisionResolutionSystem
    {
    public:
        struct SolverSettings
        {
            int   positionIterations{16};
            int   velocityIterations{8};
            float penetrationSlop{0.01f};
            float correctionPercent{0.2f};
            float maxCorrection{0.2f};
        };

        void SetSolverSettings(const SolverSettings& settings) { m_settings = settings; }

        // Resolve collisions for raw vectors (legacy/test usage)
        void Resolve(const std::vector<CollisionEvent>& events,
                     std::vector<TransformComponent>& transforms,
                     std::vector<RigidBodyComponent>& bodies) const;

        // Resolve collisions for ECS world
        void Resolve(const std::vector<CollisionEvent>& events, ecs::World& world) const;

        void ResolvePosition(const std::vector<CollisionEvent>& events, ecs::World& world, jobs::JobSystem* jobSystem = nullptr) const;
        void ResolveVelocity(const std::vector<CollisionEvent>& events, ecs::World& world, jobs::JobSystem* jobSystem = nullptr) const;

    private:
        SolverSettings m_settings{};
    };

    // Resolves constraints (joints).
    class ConstraintResolutionSystem
    {
    public:
        void Resolve(ecs::World& world, float dt) const;
        void SetIterationCount(int iterations) { m_iterations = std::max(1, iterations); }
        int  IterationCount() const noexcept { return m_iterations; }

    private:
        int m_iterations{8};
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
        PhysicsSystem();
        void Update(ecs::World& world, float dt) override;

        void SetSettings(const PhysicsSettings& settings);
        const PhysicsSettings& Settings() const noexcept { return m_settings; }

        void SetEnvironment(const EnvironmentForces& env) { m_integration.SetEnvironment(env); }
        void SetJobSystem(jobs::JobSystem* js) { m_jobSystem = js; m_integration.SetJobSystem(js); }

        const std::vector<CollisionEvent>& GetCollisionEvents() const { return m_events; }

    private:
        void ApplySettings();

        PhysicsIntegrationSystem  m_integration;
        CollisionSystem           m_collision;
        CollisionResolutionSystem m_resolution;
        ConstraintResolutionSystem m_constraints;
        std::vector<CollisionEvent> m_events;
        jobs::JobSystem*          m_jobSystem{nullptr};
        PhysicsSettings           m_settings{};
    };
}
