#include "ecs/World.hpp"
#include "physics/Systems.hpp"
#include "physics/Components.hpp"
#include <cassert>
#include <iostream>

int main(){
    ecs::World world;
    auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
    auto* physicsSystemPtr = physicsSystem.get();
    world.AddSystem(std::move(physicsSystem));

    auto e1 = world.CreateEntity();
    auto e2 = world.CreateEntity();
    world.AddComponent<physics::AABBComponent>(e1, physics::AABBComponent{0.f,0.f,2.f,2.f});
    world.AddComponent<physics::AABBComponent>(e2, physics::AABBComponent{1.f,1.f,3.f,3.f});

    world.Update(0.0f);
    assert(physicsSystemPtr->GetCollisionEvents().size() == 1);
    (void)physicsSystemPtr;
    std::cout << "ECS collision test passed\n";
    return 0;
}
