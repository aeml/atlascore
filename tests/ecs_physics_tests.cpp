#include "ecs/World.hpp"
#include "physics/EcsSystems.hpp"
#include "physics/Components.hpp"
#include <cassert>
#include <iostream>

int main(){
    ecs::World world;
    world.AddSystem(std::make_unique<physics::PhysicsEcsSystem>());
    auto e = world.CreateEntity();
    world.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{0.f, 10.f});
    world.AddComponent<physics::RigidBodyComponent>(e, physics::RigidBodyComponent{0.f, 0.f});
    // attach AABB to verify it moves with transform
    world.AddComponent<physics::AABBComponent>(e, physics::AABBComponent{-0.5f, 9.5f, 0.5f, 10.5f});
    const float dt = 1.0f/60.0f;
    world.Update(dt);
    auto* t = world.GetComponent<physics::TransformComponent>(e);
    auto* rb = world.GetComponent<physics::RigidBodyComponent>(e);
    auto* aabb = world.GetComponent<physics::AABBComponent>(e);
    assert(t && rb && aabb);
    assert(rb->vy < 0.0f);
    assert(t->y < 10.0f);
    // AABB should have moved down by same delta
    float dy = t->y - 10.0f;
    assert(aabb->minY < 9.5f && aabb->maxY < 10.5f);
    std::cout << "ECS physics test passed\n";
    return 0;
}
