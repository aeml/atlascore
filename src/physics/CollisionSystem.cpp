#include "physics/CollisionSystem.hpp"

namespace physics {

void CollisionSystem::Detect(const std::vector<AABBComponent>& aabbs, std::vector<CollisionEvent>& outEvents) const {
    outEvents.clear();
    const std::size_t n = aabbs.size();
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            if (Overlaps(aabbs[i], aabbs[j])) {
                outEvents.push_back(CollisionEvent{static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
            }
        }
    }
}

}
