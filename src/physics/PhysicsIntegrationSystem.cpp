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

#include "physics/Systems.hpp"

#include "jobs/JobSystem.hpp"

#include <algorithm>
#include <cmath>

namespace physics
{
    namespace
    {
        inline void EnsureDerivedMass(RigidBodyComponent& body)
        {
            if (body.mass <= 0.0f)
            {
                body.invMass = 0.0f;
                body.inertia = 0.0f;
                body.invInertia = 0.0f;
                return;
            }

            if (body.invMass <= 0.0f)
            {
                body.invMass = 1.0f / body.mass;
            }

            if (body.inertia <= 0.0f)
            {
                body.inertia = 0.5f * body.mass;
            }

            if (body.invInertia <= 0.0f && body.inertia > 0.0f)
            {
                body.invInertia = 1.0f / body.inertia;
            }
        }
    }

    void PhysicsIntegrationSystem::Update(ecs::World& world, float dt)
    {
        auto* rbStorage = world.GetStorage<RigidBodyComponent>();
        auto* tfStorage = world.GetStorage<TransformComponent>();
        if (!rbStorage || !tfStorage) return;

        auto& bodies = rbStorage->GetData();
        const auto& entities = rbStorage->GetEntities();
        size_t count = bodies.size();

        auto integrateRange = [&](size_t start, size_t end) {
            for (size_t i = start; i < end; ++i) {
                ecs::EntityId id = entities[i];
                TransformComponent* tf = tfStorage->Get(id);
                if (tf) {
                    auto& b = bodies[i];
                    if (b.invMass == 0.0f && b.mass > 0.0f) {
                        EnsureDerivedMass(b);
                    }
                    if (b.invMass == 0.0f) {
                        b.angularVelocity = 0.0f;
                        b.torque = 0.0f;
                        continue;
                    }

                    b.lastX = tf->x;
                    b.lastY = tf->y;
                    b.lastAngle = tf->rotation;

                    const float ax = m_env.windX - m_env.drag * b.vx;
                    const float ay = m_env.gravityY + m_env.windY - m_env.drag * b.vy;

                    b.vx += ax * dt;
                    b.vy += ay * dt;

                    const float maxVel = 50.0f;
                    float vSq = b.vx * b.vx + b.vy * b.vy;
                    if (vSq > maxVel * maxVel) {
                        float v = std::sqrt(vSq);
                        b.vx = (b.vx / v) * maxVel;
                        b.vy = (b.vy / v) * maxVel;
                    }

                    tf->x += b.vx * dt;
                    tf->y += b.vy * dt;

                    if (b.invInertia == 0.0f && b.inertia > 0.0f)
                    {
                        b.invInertia = 1.0f / b.inertia;
                    }
                    if (b.invInertia > 0.0f)
                    {
                        float angularAccel = b.torque * b.invInertia - b.angularDrag * b.angularVelocity;
                        b.angularVelocity += angularAccel * dt;
                        const float damping = std::max(0.0f, 1.0f - b.angularFriction * dt);
                        b.angularVelocity *= damping;
                        tf->rotation += b.angularVelocity * dt;
                    }
                    else
                    {
                        b.angularVelocity = 0.0f;
                    }
                    b.torque = 0.0f;

                }
            }
        };

        if (m_jobSystem && count > 256) {
             const std::size_t batchSize = std::max(std::size_t(64), count / (m_jobSystem->WorkerCount() * 4));
             auto handles = m_jobSystem->Dispatch(count, batchSize, integrateRange);
             m_jobSystem->Wait(handles);
        } else {
            integrateRange(0, count);
        }
    }

    void PhysicsIntegrationSystem::Integrate(std::vector<TransformComponent>& transforms,
                                             std::vector<RigidBodyComponent>& bodies,
                                             float                            dt) const
    {
        const std::size_t count = std::min(transforms.size(), bodies.size());

        auto integrateRange = [&](std::size_t start, std::size_t end)
        {
            for (std::size_t i = start; i < end; ++i)
            {
                auto& t = transforms[i];
                auto& b = bodies[i];
                EnsureDerivedMass(b);

                const float ax = m_env.windX - m_env.drag * b.vx;
                const float ay = m_env.gravityY + m_env.windY - m_env.drag * b.vy;

                b.vx += ax * dt;
                b.vy += ay * dt;

                const float maxVel = 50.0f;
                float vSq = b.vx * b.vx + b.vy * b.vy;
                if (vSq > maxVel * maxVel) {
                    float v = std::sqrt(vSq);
                    b.vx = (b.vx / v) * maxVel;
                    b.vy = (b.vy / v) * maxVel;
                }

                t.x += b.vx * dt;
                t.y += b.vy * dt;

                if (b.invInertia == 0.0f && b.inertia > 0.0f)
                {
                    b.invInertia = 1.0f / b.inertia;
                }
                if (b.invInertia > 0.0f)
                {
                    float angularAccel = b.torque * b.invInertia - b.angularDrag * b.angularVelocity;
                    b.angularVelocity += angularAccel * dt;
                    const float damping = std::max(0.0f, 1.0f - b.angularFriction * dt);
                    b.angularVelocity *= damping;
                    t.rotation += b.angularVelocity * dt;
                }
                else
                {
                    b.angularVelocity = 0.0f;
                }
                b.torque = 0.0f;
            }
        };

        if (m_jobSystem && count > 256)
        {
            const std::size_t batchSize = std::max(std::size_t(64), count / (m_jobSystem->WorkerCount() * 4));
            auto handles = m_jobSystem->Dispatch(count, batchSize, integrateRange);
            m_jobSystem->Wait(handles);
        }
        else
        {
            integrateRange(0, count);
        }
    }

    void PhysicsIntegrationSystem::UpdateVelocities(ecs::World& world, float dt)
    {
        if (!std::isfinite(dt) || dt <= 0.0f)
        {
            return;
        }

        auto* rbStorage = world.GetStorage<RigidBodyComponent>();
        auto* tfStorage = world.GetStorage<TransformComponent>();
        if (!rbStorage || !tfStorage) return;

        auto& bodies = rbStorage->GetData();
        const auto& entities = rbStorage->GetEntities();
        size_t count = bodies.size();

        for (size_t i = 0; i < count; ++i) {
            ecs::EntityId id = entities[i];
            TransformComponent* tf = tfStorage->Get(id);
            if (tf) {
                auto& b = bodies[i];
                if (b.invMass == 0.0f) continue;

                b.vx = (tf->x - b.lastX) / dt;
                b.vy = (tf->y - b.lastY) / dt;
                b.angularVelocity = (tf->rotation - b.lastAngle) / dt;

                const float maxVel = 50.0f;
                float vSq = b.vx * b.vx + b.vy * b.vy;
                if (vSq > maxVel * maxVel) {
                    float v = std::sqrt(vSq);
                    b.vx = (b.vx / v) * maxVel;
                    b.vy = (b.vy / v) * maxVel;
                }
            }
        }
    }
}
