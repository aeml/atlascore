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

    // Axis-aligned bounding box for simple collision tests.
    struct AABBComponent
    {
        float minX{0.0f};
        float minY{0.0f};
        float maxX{0.0f};
        float maxY{0.0f};
    };
}
