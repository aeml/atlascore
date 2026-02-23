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
#include "physics/Systems.hpp"
#include "physics/Components.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main(){
    ecs::World world;
    world.AddSystem(std::make_unique<physics::PhysicsSystem>());
    auto e = world.CreateEntity();
    world.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{0.f, 10.f});
    world.AddComponent<physics::RigidBodyComponent>(e, physics::RigidBodyComponent{0.f, 0.f});
    // attach AABB to verify it moves with transform
    world.AddComponent<physics::AABBComponent>(e, physics::AABBComponent{-0.5f, 9.5f, 0.5f, 10.5f});

    // Zero timestep should not introduce NaN/Inf values.
    world.Update(0.0f);
    auto* t = world.GetComponent<physics::TransformComponent>(e);
    auto* rb = world.GetComponent<physics::RigidBodyComponent>(e);
    assert(t && rb);
    assert(std::isfinite(t->x) && std::isfinite(t->y));
    assert(std::isfinite(rb->vx) && std::isfinite(rb->vy));
    assert(t->x == 0.0f && t->y == 10.0f);

    const float dt = 1.0f/60.0f;
    world.Update(dt);

    auto* aabb = world.GetComponent<physics::AABBComponent>(e);
    assert(t && rb && aabb);
    assert(rb->vy < 0.0f);
    assert(t->y < 10.0f);
    // AABB should have moved down by same delta
    float dy = t->y - 10.0f;
    assert(aabb->minY < 9.5f && aabb->maxY < 10.5f);
    (void)rb; (void)aabb; (void)dy;
    std::cout << "ECS physics test passed\n";
    return 0;
}
