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

namespace
{
    std::unique_ptr<physics::PhysicsSystem> CreatePhysicsSystem()
    {
        auto system = std::make_unique<physics::PhysicsSystem>();
        physics::PhysicsSettings settings;
        settings.substeps = 1;
        system->SetSettings(settings);
        return system;
    }

    void VerifyCircleCircle()
    {
        ecs::World world;
        auto physicsSystem = CreatePhysicsSystem();
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

        const auto& events = physicsPtr->GetCollisionEvents();
        bool foundPair = false;
        for (const auto& event : events)
        {
            if ((event.entityA == a && event.entityB == b) ||
                (event.entityA == b && event.entityB == a))
            {
                foundPair = true;
                break;
            }
        }
        assert(foundPair && "Circle-only entities should enter broadphase detection");

        auto* tfA = world.GetComponent<physics::TransformComponent>(a);
        auto* tfB = world.GetComponent<physics::TransformComponent>(b);
        assert(tfA && tfB);
        const float separation = std::abs(tfB->x - tfA->x);
        assert(separation >= 1.95f && "Circle-only entities should collide and separate");
    }

    void VerifyCircleWall()
    {
        ecs::World world;
        auto physicsSystem = CreatePhysicsSystem();
        auto* physicsPtr = physicsSystem.get();
        world.AddSystem(std::move(physicsSystem));

        auto wall = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(wall, physics::TransformComponent{0.0f, 0.0f, 0.0f});
        auto& wallRb = world.AddComponent<physics::RigidBodyComponent>(wall);
        wallRb.mass = 0.0f;
        wallRb.invMass = 0.0f;
        world.AddComponent<physics::AABBComponent>(wall, -1.0f, -1.0f, 1.0f, 1.0f);

        auto ball = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(ball, physics::TransformComponent{1.6f, 0.0f, 0.0f});
        auto& ballRb = world.AddComponent<physics::RigidBodyComponent>(ball);
        ballRb.mass = 1.0f;
        ballRb.invMass = 1.0f;
        ballRb.vx = -2.0f;
        world.AddComponent<physics::CircleColliderComponent>(ball, 0.75f);

        world.Update(1.0f / 60.0f);

        const auto& events = physicsPtr->GetCollisionEvents();
        bool foundPair = false;
        for (const auto& event : events)
        {
            if ((event.entityA == wall && event.entityB == ball) ||
                (event.entityA == ball && event.entityB == wall))
            {
                foundPair = true;
                break;
            }
        }
        assert(foundPair && "Circle-vs-wall should be detected without particle AABBs");
    }
}

int main()
{
    VerifyCircleCircle();
    VerifyCircleWall();
    std::cout << "Physics circle broadphase tests passed\n";
    return 0;
}
