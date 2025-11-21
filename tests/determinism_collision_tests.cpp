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

#include "simlab/WorldHasher.hpp"
#include "physics/CollisionSystem.hpp"
#include "physics/Components.hpp"
#include <cassert>
#include <vector>
#include <iostream>

int main() {
    simlab::WorldHasher hasher;
    physics::CollisionSystem collision;

    std::vector<physics::AABBComponent> boxesA(2), boxesB(2);
    // Identical initialization ensures determinism.
    boxesA[0] = boxesB[0] = {0.f,0.f,2.f,2.f};
    boxesA[1] = boxesB[1] = {1.f,1.f,3.f,3.f};

    std::vector<physics::CollisionEvent> eventsA, eventsB;
    collision.Detect(boxesA, eventsA);
    collision.Detect(boxesB, eventsB);

    // Hash AABBs + event count to verify determinism across duplicate sets.
    auto hA = hasher.HashAABBs(boxesA);
    auto hB = hasher.HashAABBs(boxesB);
    // Combine with event counts
    hA = hasher.Combine(hA, static_cast<std::uint64_t>(eventsA.size()));
    hB = hasher.Combine(hB, static_cast<std::uint64_t>(eventsB.size()));
    assert(hA == hB);
    assert(eventsA.size() == 1 && eventsB.size() == 1);

    std::cout << "Determinism collision tests passed.\n";
    return 0;
}
