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
                    if (b.invMass == 0.0f) continue;

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

        // 1. Velocity Solver (Iterative)
        const int velocityIterations = 8;
        for (int i = 0; i < velocityIterations; ++i)
        {
            for (const auto& event : events)
            {
                if (event.indexA >= entities.size() || event.indexB >= entities.size()) continue;

                ecs::EntityId idA = entities[event.indexA];
                ecs::EntityId idB = entities[event.indexB];

                auto* bA = rbStorage->Get(idA);
                auto* bB = rbStorage->Get(idB);

                if (!bA || !bB) continue;

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

                // Friction
                float tx = -event.normalY;
                float ty = event.normalX;
                float velAlongTangent = rvx * tx + rvy * ty;

                float jt = -velAlongTangent;
                jt /= (bA->invMass + bB->invMass);

                // Coulomb friction
                float mu = std::sqrt(bA->friction * bA->friction + bB->friction * bB->friction);
                float maxJt = mu * j;
                if (std::abs(jt) > maxJt)
                {
                    jt = (jt > 0) ? maxJt : -maxJt;
                }

                float frictionImpulseX = jt * tx;
                float frictionImpulseY = jt * ty;

                bA->vx -= (impulseX + frictionImpulseX) * bA->invMass;
                bA->vy -= (impulseY + frictionImpulseY) * bA->invMass;
                bB->vx += (impulseX + frictionImpulseX) * bB->invMass;
                bB->vy += (impulseY + frictionImpulseY) * bB->invMass;
            }
        }

        // 2. Position Correction (Linear Projection)
        // Iterate to propagate corrections up the stack
        const int positionIterations = 4;
        const float percent = 0.5f;
        const float slop = 0.01f;
        
        for (int i = 0; i < positionIterations; ++i)
        {
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

                // Positional correction (Linear Projection)
                float correction = std::max(event.penetration - slop, 0.0f) / (bA->invMass + bB->invMass) * percent;
                
                float cx = correction * event.normalX;
                float cy = correction * event.normalY;

                float dxA = -cx * bA->invMass;
                float dyA = -cy * bA->invMass;
                float dxB = cx * bB->invMass;
                float dyB = cy * bB->invMass;

                tA->x += dxA;
                tA->y += dyA;
                tB->x += dxB;
                tB->y += dyB;

                // Sync AABBs
                if (auto* boxA = aabbStorage->Get(idA)) {
                    boxA->minX += dxA; boxA->maxX += dxA;
                    boxA->minY += dyA; boxA->maxY += dyA;
                }
                if (auto* boxB = aabbStorage->Get(idB)) {
                    boxB->minX += dxB; boxB->maxX += dxB;
                    boxB->minY += dyB; boxB->maxY += dyB;
                }
            }
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
        const int substeps = 8;
        const float subDt = dt / substeps;

        for (int i = 0; i < substeps; ++i)
        {
            // 1. Integration
            m_integration.Update(world, subDt);

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
}
