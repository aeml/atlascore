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

#include <algorithm>
#include <cstdint>

namespace physics
{
    struct TransformComponent
    {
        float x{0.0f};
        float y{0.0f};
        float rotation{0.0f};
    };

    struct RigidBodyComponent
    {
        float vx{0.0f};
        float vy{0.0f};
        float lastX{0.0f}; // For PBD
        float lastY{0.0f}; // For PBD
        float lastAngle{0.0f};
        float mass{1.0f};
        float invMass{1.0f}; // 1.0 / mass, 0.0 for static bodies
        float inertia{1.0f};
        float invInertia{1.0f};
        float restitution{0.5f}; // Bounciness [0, 1]
        float friction{0.5f}; // Friction coefficient [0, 1]
        float angularVelocity{0.0f};
        float torque{0.0f};
        float angularFriction{0.5f};
        float angularDrag{0.0f};
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

    inline void ConfigureCircleInertia(RigidBodyComponent& body, float radius)
    {
        if (body.mass <= 0.0f || radius <= 0.0f)
        {
            body.inertia = 0.0f;
            body.invInertia = 0.0f;
            return;
        }
        const float r = radius;
        body.inertia = 0.5f * body.mass * r * r;
        body.invInertia = body.inertia > 0.0f ? 1.0f / body.inertia : 0.0f;
    }

    inline void ConfigureBoxInertia(RigidBodyComponent& body, float width, float height)
    {
        if (body.mass <= 0.0f)
        {
            body.inertia = 0.0f;
            body.invInertia = 0.0f;
            return;
        }
        const float w = width;
        const float h = height;
        body.inertia = (body.mass / 12.0f) * (w * w + h * h);
        body.invInertia = body.inertia > 0.0f ? 1.0f / body.inertia : 0.0f;
    }
}
