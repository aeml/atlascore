#pragma once

#include <cstdint>

namespace physics
{
    struct TransformComponent
    {
        float x{0.0f};
        float y{0.0f};
    };

    struct RigidBodyComponent
    {
        float vx{0.0f};
        float vy{0.0f};
    };

    // Simple environment-wide forces for weather-like simulations.
    struct EnvironmentForces
    {
        float gravityY{-9.81f}; // downward acceleration
        float windX{0.0f};      // horizontal wind
        float windY{0.0f};      // vertical wind / updrafts
        float drag{0.0f};       // linear drag coefficient
    };

    // Axis-aligned bounding box for simple collision tests.
    struct AABBComponent
    {
        float minX{0.0f};
        float minY{0.0f};
        float maxX{0.0f};
        float maxY{0.0f};
    };
}
