#include "physics/Systems.hpp"

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
        for (std::size_t i = 0; i < count; ++i)
        {
            auto& t = transforms[i];
            auto& b = bodies[i];
            // Gravity affects vertical velocity (y-axis).
            b.vy += m_gravity * dt;
            // Integrate position.
            t.x += b.vx * dt;
            t.y += b.vy * dt;
        }
    }
}
