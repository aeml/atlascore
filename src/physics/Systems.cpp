#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"

#include <algorithm>

namespace physics
{
    void PhysicsIntegrationSystem::Update(ecs::World& world, float dt)
    {
        auto* rbStorage = world.GetStorage<RigidBodyComponent>();
        auto* tfStorage = world.GetStorage<TransformComponent>();
        auto* aabbStorage = world.GetStorage<AABBComponent>();

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

                    float oldX = tf->x;
                    float oldY = tf->y;

                    tf->x += b.vx * dt;
                    tf->y += b.vy * dt;

                    // Update AABB if present
                    if (aabbStorage) {
                        if (auto* aabb = aabbStorage->Get(id)) {
                            float dx = tf->x - oldX;
                            float dy = tf->y - oldY;
                            aabb->minX += dx;
                            aabb->maxX += dx;
                            aabb->minY += dy;
                            aabb->maxY += dy;
                        }
                    }
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

    void CollisionResolutionSystem::Resolve(const std::vector<CollisionEvent>& events, ecs::World& world) const
    {
        auto* tfStorage = world.GetStorage<TransformComponent>();
        auto* rbStorage = world.GetStorage<RigidBodyComponent>();
        auto* aabbStorage = world.GetStorage<AABBComponent>();

        if (!tfStorage || !rbStorage || !aabbStorage) return;

        const auto& entities = aabbStorage->GetEntities();

        for (const auto& event : events)
        {
            if (event.indexA >= entities.size() || event.indexB >= entities.size()) continue;

            ecs::EntityId idA = entities[event.indexA];
            ecs::EntityId idB = entities[event.indexB];

            auto* tA = tfStorage->Get(idA);
            auto* bA = rbStorage->Get(idA);
            auto* tB = tfStorage->Get(idB);
            auto* bB = rbStorage->Get(idB);

            if (!tA || !bA || !tB || !bB) continue;

            // Relative velocity
            float rvx = bB->vx - bA->vx;
            float rvy = bB->vy - bA->vy;

            // Velocity along normal
            float velAlongNormal = rvx * event.normalX + rvy * event.normalY;

            // Do not resolve if velocities are separating
            if (velAlongNormal > 0)
                continue;

            // Restitution (min of both)
            float e = std::min(bA->restitution, bB->restitution);

            // Impulse scalar
            float j = -(1 + e) * velAlongNormal;
            j /= (bA->invMass + bB->invMass);

            // Apply impulse
            float impulseX = j * event.normalX;
            float impulseY = j * event.normalY;

            bA->vx -= impulseX * bA->invMass;
            bA->vy -= impulseY * bA->invMass;
            bB->vx += impulseX * bB->invMass;
            bB->vy += impulseY * bB->invMass;

            // Positional correction (Linear Projection)
            const float percent = 0.2f;
            const float slop = 0.01f;
            float correction = std::max(event.penetration - slop, 0.0f) / (bA->invMass + bB->invMass) * percent;
            
            float cx = correction * event.normalX;
            float cy = correction * event.normalY;

            tA->x -= cx * bA->invMass;
            tA->y -= cy * bA->invMass;
            tB->x += cx * bB->invMass;
            tB->y += cy * bB->invMass;
        }
    }

    void CollisionResolutionSystem::Resolve(const std::vector<CollisionEvent>& events,
                                            std::vector<TransformComponent>& transforms,
                                            std::vector<RigidBodyComponent>& bodies) const
    {
        for (const auto& event : events)
        {
            if (event.indexA >= transforms.size() || event.indexB >= transforms.size()) continue;

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
            const float percent = 0.2f;
            const float slop = 0.01f;
            float correction = std::max(event.penetration - slop, 0.0f) / (bA.invMass + bB.invMass) * percent;
            
            float cx = correction * event.normalX;
            float cy = correction * event.normalY;

            tA.x -= cx * bA.invMass;
            tA.y -= cy * bA.invMass;
            tB.x += cx * bB.invMass;
            tB.y += cy * bB.invMass;
        }
    }

    void PhysicsSystem::Update(ecs::World& world, float dt)
    {
        // 1. Integration
        m_integration.Update(world, dt);

        // 2. Detection
        auto* aabbStorage = world.GetStorage<AABBComponent>();
        if (aabbStorage) {
            const auto& aabbs = aabbStorage->GetData();
            m_collision.Detect(aabbs, m_events, m_jobSystem);
        }

        // 3. Resolution
        if (!m_events.empty()) {
            m_resolution.Resolve(m_events, world);
        }
    }
}
