#pragma once

#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/CollisionSystem.hpp"
#include "physics/Systems.hpp"

#include <memory>
#include <vector>

namespace physics {

class PhysicsEcsSystem : public ecs::ISystem {
public:
    PhysicsEcsSystem() = default; // default: uses zeroed EnvironmentForces
    explicit PhysicsEcsSystem(const PhysicsIntegrationSystem* integration)
        : m_integration(integration) {}

    void Update(ecs::World& world, float dt) override {
        const EnvironmentForces env = m_integration ? m_integration->Environment()
                                                    : EnvironmentForces{};

        world.ForEach<physics::TransformComponent>([&](ecs::EntityId id, physics::TransformComponent& t){
            auto* rb   = world.GetComponent<physics::RigidBodyComponent>(id);
            auto* aabb = world.GetComponent<physics::AABBComponent>(id);
            if (!rb || !aabb) return;

            float oldX = t.x;
            float oldY = t.y;

            const float ax = env.windX - env.drag * rb->vx;
            const float ay = env.gravityY + env.windY - env.drag * rb->vy;

            rb->vx += ax * dt;
            rb->vy += ay * dt;

            t.x += rb->vx * dt;
            t.y += rb->vy * dt;

            const float dx = t.x - oldX;
            const float dy = t.y - oldY;
            aabb->minX += dx; aabb->maxX += dx;
            aabb->minY += dy; aabb->maxY += dy;
        });
    }

private:
    const PhysicsIntegrationSystem* m_integration{};
};

class CollisionEcsSystem : public ecs::ISystem {
public:
    void Update(ecs::World& world, float /*dt*/) override {
        boxes.clear();
        ids.clear();
        world.ForEach<physics::AABBComponent>([&](ecs::EntityId id, physics::AABBComponent& b){
            ids.push_back(id);
            boxes.push_back(b);
        });
        events.clear();
        detector.Detect(boxes, events);
    }

    const std::vector<physics::CollisionEvent>& Events() const { return events; }

private:
    std::vector<physics::AABBComponent> boxes;
    std::vector<ecs::EntityId> ids;
    std::vector<physics::CollisionEvent> events;
    physics::CollisionSystem detector;
};

} // namespace physics
