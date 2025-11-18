#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"

#include <algorithm>

namespace physics
{
    void PhysicsIntegrationSystem::Update(ecs::World& world, float dt)
    {
        (void)world;
        (void)dt;
        // Future ECS integration will enumerate entities/components here.
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
}
