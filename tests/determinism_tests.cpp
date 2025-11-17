#include "simlab/WorldHasher.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "core/Logger.hpp"

#include <cassert>
#include <vector>

int main()
{
    core::Logger logger;
    logger.Info("Running determinism tests...");

    physics::PhysicsIntegrationSystem physics;
    simlab::WorldHasher               hasher;

    std::vector<physics::TransformComponent> tA(2), tB(2);
    std::vector<physics::RigidBodyComponent> bA(2), bB(2);

    // Initialize both sets identically.
    for (int i = 0; i < 2; ++i)
    {
        tA[i].x = tB[i].x = static_cast<float>(i);
        tA[i].y = tB[i].y = 5.0f + static_cast<float>(i);
        bA[i].vx = bB[i].vx = 0.0f;
        bA[i].vy = bB[i].vy = 0.0f;
    }

    const int steps = 10;
    const float dt = 1.0f / 60.0f;

    for (int s = 0; s < steps; ++s)
    {
        physics.Integrate(tA, bA, dt);
        physics.Integrate(tB, bB, dt);
        const auto hA = hasher.HashBodies(tA, bA);
        const auto hB = hasher.HashBodies(tB, bB);
        assert(hA == hB);
    }

    logger.Info("[PASS] Determinism hashes matched");
    return 0;
}
