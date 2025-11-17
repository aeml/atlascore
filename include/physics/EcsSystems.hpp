#pragma once

#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/CollisionSystem.hpp"

#include <memory>
#include <vector>

namespace physics {

class PhysicsEcsSystem : public ecs::ISystem {
public:
    void Update(ecs::World& world, float dt) override {
        const float g = -9.81f;
        world.ForEach<physics::TransformComponent>([&](ecs::EntityId id, physics::TransformComponent& t){
            auto* rb = world.GetComponent<physics::RigidBodyComponent>(id);
            float oldX = t.x; float oldY = t.y;
            if (rb) {
                rb->vy += g * dt;
                t.x += rb->vx * dt;
                t.y += rb->vy * dt;
            }
            float dx = t.x - oldX; float dy = t.y - oldY;
            if (auto* aabb = world.GetComponent<physics::AABBComponent>(id)) {
                aabb->minX += dx; aabb->maxX += dx;
                aabb->minY += dy; aabb->maxY += dy;
            }
            // Simple ground plane at y=0
            if (t.y < 0.0f) {
                float correction = -t.y;
                t.y = 0.0f; if (rb) rb->vy = 0.0f;
                if (auto* aabb = world.GetComponent<physics::AABBComponent>(id)) {
                    aabb->minY += correction; aabb->maxY += correction;
                }
            }
        });
    }
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
