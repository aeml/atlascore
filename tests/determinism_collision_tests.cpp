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
