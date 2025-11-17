#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include <cassert>
#include <iostream>

int main() {
    ecs::World world;
    auto e1 = world.CreateEntity();
    auto e2 = world.CreateEntity();
    world.AddComponent<physics::TransformComponent>(e1, physics::TransformComponent{1.f,2.f});
    world.AddComponent<physics::TransformComponent>(e2, physics::TransformComponent{3.f,4.f});
    // Retrieval for each entity returns distinct components.
    auto* t1 = world.GetComponent<physics::TransformComponent>(e1);
    auto* t2 = world.GetComponent<physics::TransformComponent>(e2);
    assert(t1 && t2);
    assert(t1->x == 1.f && t1->y == 2.f);
    assert(t2->x == 3.f && t2->y == 4.f);
    // Missing component returns nullptr.
    auto* rbMissing = world.GetComponent<physics::RigidBodyComponent>(e1);
    assert(rbMissing == nullptr);
    std::cout << "ECS extra tests passed.\n";
    return 0;
}
