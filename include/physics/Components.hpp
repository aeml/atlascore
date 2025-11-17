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
}
