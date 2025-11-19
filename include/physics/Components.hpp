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
        float lastX{0.0f}; // For PBD
        float lastY{0.0f}; // For PBD
        float mass{1.0f};
        float invMass{1.0f}; // 1.0 / mass, 0.0 for static bodies
        float restitution{0.5f}; // Bounciness [0, 1]
        float friction{0.5f}; // Friction coefficient [0, 1]
    };

    // Simple environment-wide forces for weather-like simulations.
    struct EnvironmentForces
    {
        float gravityY{-9.81f}; // downward acceleration
        float windX{0.0f};      // horizontal wind
        float windY{0.0f};      // vertical wind / updrafts
        float drag{0.0f};       // linear drag coefficient
    };

    struct DistanceJointComponent
    {
        std::uint32_t entityA;
        std::uint32_t entityB;
        float targetDistance;
        float compliance{0.0f}; // 0 = rigid, >0 = soft (inverse stiffness)
    };

    // Axis-aligned bounding box for simple collision tests.
    struct AABBComponent
    {
        float minX{0.0f};
        float minY{0.0f};
        float maxX{0.0f};
        float maxY{0.0f};
    };

    struct CircleColliderComponent
    {
        float radius{1.0f};
        float offsetX{0.0f};
        float offsetY{0.0f};
    };
}
