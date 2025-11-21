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
    (void)t1; (void)t2;
    // Missing component returns nullptr.
    auto* rbMissing = world.GetComponent<physics::RigidBodyComponent>(e1);
    assert(rbMissing == nullptr);
    (void)rbMissing;
    std::cout << "ECS extra tests passed.\n";
    return 0;
}
