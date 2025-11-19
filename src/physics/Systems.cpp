#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"

#include <algorithm>

namespace physics
{
    void PhysicsIntegrationSystem::Update(ecs::World& world, float dt)
    {
        auto* rbStorage = world.GetStorage<RigidBodyComponent>();
        auto* tfStorage = world.GetStorage<TransformComponent>();

        if (!rbStorage || !tfStorage) return;

        auto& bodies = rbStorage->GetData();
        const auto& entities = rbStorage->GetEntities();
        size_t count = bodies.size();

        auto integrateRange = [&](size_t start, size_t end) {
            for (size_t i = start; i < end; ++i) {
                ecs::EntityId id = entities[i];
                TransformComponent* tf = tfStorage->Get(id);
                if (tf) {
                    auto& b = bodies[i];
                    const float ax = m_env.windX - m_env.drag * b.vx;
                    const float ay = m_env.gravityY + m_env.windY - m_env.drag * b.vy;

                    b.vx += ax * dt;
                    b.vy += ay * dt;

                    tf->x += b.vx * dt;
                    tf->y += b.vy * dt;
                }
            }
        };

        if (m_jobSystem && count > 256) {
             const std::size_t batchSize = std::max(std::size_t(64), count / (m_jobSystem->WorkerCount() * 4));
             auto handles = m_jobSystem->Dispatch(count, batchSize, integrateRange);
             m_jobSystem->Wait(handles);
        } else {
            integrateRange(0, count);
        }
    }

    void PhysicsIntegrationSystem::Integrate(std::vector<TransformComponent>& transforms,
                                             std::vector<RigidBodyComponent>& bodies,
                                             float                            dt) const
    {
        const std::size_t count = std::min(transforms.size(), bodies.size());

        auto integrateRange = [&](std::size_t start, std::size_t end)
        {
            for (std::size_t i = start; i < end; ++i)
            {
                auto& t = transforms[i];
                auto& b = bodies[i];
                // Apply simple environment forces: gravity, wind, and linear drag.
                const float ax = m_env.windX - m_env.drag * b.vx;
                const float ay = m_env.gravityY + m_env.windY - m_env.drag * b.vy;

                b.vx += ax * dt;
                b.vy += ay * dt;

                // Integrate position.
                t.x += b.vx * dt;
                t.y += b.vy * dt;
            }
        };

        if (m_jobSystem && count > 256)
        {
            const std::size_t batchSize = std::max(std::size_t(64), count / (m_jobSystem->WorkerCount() * 4));
            auto handles = m_jobSystem->Dispatch(count, batchSize, integrateRange);
            m_jobSystem->Wait(handles);
        }
        else
        {
            integrateRange(0, count);
        }
    }

    void CollisionResolutionSystem::Resolve(const std::vector<CollisionEvent>& events,
                                            std::vector<TransformComponent>& transforms,
                                            std::vector<RigidBodyComponent>& bodies) const
    {
        for (const auto& event : events)
        {
            auto& tA = transforms[event.indexA];
            auto& bA = bodies[event.indexA];
            auto& tB = transforms[event.indexB];
            auto& bB = bodies[event.indexB];

            // Relative velocity
            float rvx = bB.vx - bA.vx;
            float rvy = bB.vy - bA.vy;

            // Velocity along normal
            float velAlongNormal = rvx * event.normalX + rvy * event.normalY;

            // Do not resolve if velocities are separating
            if (velAlongNormal > 0)
                continue;

            // Restitution (min of both)
            float e = std::min(bA.restitution, bB.restitution);

            // Impulse scalar
            float j = -(1 + e) * velAlongNormal;
            j /= (bA.invMass + bB.invMass);

            // Apply impulse
            float impulseX = j * event.normalX;
            float impulseY = j * event.normalY;

            bA.vx -= impulseX * bA.invMass;
            bA.vy -= impulseY * bA.invMass;
            bB.vx += impulseX * bB.invMass;
            bB.vy += impulseY * bB.invMass;

            // Positional correction (Linear Projection)
            const float percent = 0.2f; // usually 20% to 80%
            const float slop = 0.01f; // usually 0.01 to 0.1
            float correction = std::max(event.penetration - slop, 0.0f) / (bA.invMass + bB.invMass) * percent;
            
            float cx = correction * event.normalX;
            float cy = correction * event.normalY;

            tA.x -= cx * bA.invMass;
            tA.y -= cy * bA.invMass;
            tB.x += cx * bB.invMass;
            tB.y += cy * bB.invMass;
        }
    }
}
