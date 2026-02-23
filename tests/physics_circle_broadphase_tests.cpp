/*
 * Copyright (C) 2025 aeml
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

int main()
{
    ecs::World world;
    auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
    auto* physicsPtr = physicsSystem.get();
    world.AddSystem(std::move(physicsSystem));

    auto a = world.CreateEntity();
    world.AddComponent<physics::TransformComponent>(a, physics::TransformComponent{0.0f, 0.0f, 0.0f});
    auto& rbA = world.AddComponent<physics::RigidBodyComponent>(a);
    rbA.mass = 1.0f;
    rbA.invMass = 1.0f;
    world.AddComponent<physics::CircleColliderComponent>(a, 1.0f);

    auto b = world.CreateEntity();
    world.AddComponent<physics::TransformComponent>(b, physics::TransformComponent{1.5f, 0.0f, 0.0f});
    auto& rbB = world.AddComponent<physics::RigidBodyComponent>(b);
    rbB.mass = 1.0f;
    rbB.invMass = 1.0f;
    world.AddComponent<physics::CircleColliderComponent>(b, 1.0f);

    world.Update(1.0f / 60.0f);

    auto* tfA = world.GetComponent<physics::TransformComponent>(a);
    auto* tfB = world.GetComponent<physics::TransformComponent>(b);
    assert(tfA && tfB);

    const float separation = std::abs(tfB->x - tfA->x);
    assert(separation >= 1.95f && "Circle-only entities should collide and separate");

    const auto& events = physicsPtr->GetCollisionEvents();
    (void)events;

    std::cout << "Physics circle broadphase tests passed\n";
    return 0;
}
