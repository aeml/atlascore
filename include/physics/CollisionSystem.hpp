#pragma once

#include <vector>
#include <utility>
#include <cstdint>

#include "physics/Components.hpp"

namespace physics {

struct CollisionEvent {
    std::uint32_t indexA; // index into component arrays
    std::uint32_t indexB;
};

class CollisionSystem {
public:
    // Detect collisions between AABBs; returns events (clears previous contents).
    void Detect(const std::vector<AABBComponent>& aabbs, std::vector<CollisionEvent>& outEvents) const;
private:
    static bool Overlaps(const AABBComponent& a, const AABBComponent& b) {
        return !(a.maxX < b.minX || b.maxX < a.minX || a.maxY < b.minY || b.maxY < a.minY);
    }
};

} // namespace physics
