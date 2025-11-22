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

#pragma once

#include <vector>
#include <utility>
#include <cstdint>

#include "physics/Components.hpp"

namespace jobs { class JobSystem; }

namespace physics {

struct CollisionEvent {
    std::uint32_t entityA; // EntityId
    std::uint32_t entityB; // EntityId
    float normalX{0.0f};
    float normalY{0.0f};
    float penetration{0.0f};
};

class CollisionSystem {
public:
    // Detect collisions between AABBs; returns events (clears previous contents).
    // Requires a parallel vector of EntityIds to populate the events correctly.
    void Detect(const std::vector<AABBComponent>& aabbs, 
                const std::vector<std::uint32_t>& entityIds,
                std::vector<CollisionEvent>& outEvents, 
                jobs::JobSystem* jobSystem = nullptr) const;

private:
    static bool Overlaps(const AABBComponent& a, const AABBComponent& b) {
        return !(a.maxX < b.minX || b.maxX < a.minX || a.maxY < b.minY || b.maxY < a.minY);
    }
};

} // namespace physics
