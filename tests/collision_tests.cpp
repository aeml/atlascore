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

#include "physics/CollisionSystem.hpp"
#include <cassert>
#include <iostream>

int main() {
    physics::CollisionSystem system;
    std::vector<physics::AABBComponent> boxes(3);

    // Box 0 overlaps with Box 1; Box 2 separate.
    boxes[0] = {0.f, 0.f, 2.f, 2.f};
    boxes[1] = {1.f, 1.f, 3.f, 3.f};
    boxes[2] = {5.f, 5.f, 6.f, 6.f};

    std::vector<physics::CollisionEvent> events;
    std::vector<std::uint32_t> ids = {0, 1, 2};
    system.Detect(boxes, ids, events);
    assert(events.size() == 1);
    assert(events[0].entityA == 0 && events[0].entityB == 1);

    // Move box 2 to overlap with box 1 only.
    boxes[2] = {2.2f, 2.2f, 4.f, 4.f};
    system.Detect(boxes, ids, events);
    // Expect collisions: (0,1) and (1,2). (0,2) not overlapping (2.2 min inside >2 max?). Actually 0 maxX=2 < 2.2 minX => no.
    assert(events.size() == 2);

    // Separate all boxes.
    boxes[0] = {0.f,0.f,1.f,1.f};
    boxes[1] = {3.f,3.f,4.f,4.f};
    boxes[2] = {6.f,6.f,7.f,7.f};
    system.Detect(boxes, ids, events);
    assert(events.empty());

    std::cout << "Collision tests passed.\n";
    return 0;
}
