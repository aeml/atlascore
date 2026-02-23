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

#include <algorithm>
#include <cmath>

namespace physics
{
    namespace
    {
        void SyncDynamicAabbsToTransforms(ecs::World& world)
        {
            auto* aabbStorage = world.GetStorage<AABBComponent>();
            auto* tfStorage = world.GetStorage<TransformComponent>();
            auto* rbStorage = world.GetStorage<RigidBodyComponent>();
            if (!aabbStorage || !tfStorage || !rbStorage)
            {
                return;
            }

            auto& aabbs = aabbStorage->GetData();
            const auto& entities = aabbStorage->GetEntities();
            const size_t count = aabbs.size();
            for (size_t i = 0; i < count; ++i)
            {
                const ecs::EntityId id = entities[i];
                auto* rb = rbStorage->Get(id);
                auto* tf = tfStorage->Get(id);
                if (!rb || rb->invMass == 0.0f || !tf)
                {
                    continue;
                }

                auto& aabb = aabbs[i];
                const float halfW = std::max(0.0f, (aabb.maxX - aabb.minX) * 0.5f);
                const float halfH = std::max(0.0f, (aabb.maxY - aabb.minY) * 0.5f);
                aabb.minX = tf->x - halfW;
                aabb.maxX = tf->x + halfW;
                aabb.minY = tf->y - halfH;
                aabb.maxY = tf->y + halfH;
            }
        }
    }

    PhysicsSystem::PhysicsSystem()
    {
        ApplySettings();
    }

    void PhysicsSystem::Update(ecs::World& world, float dt)
    {
        if (!std::isfinite(dt) || dt < 0.0f)
        {
            return;
        }

        const int substeps = std::max(1, m_settings.substeps);
        const float subDt = dt / static_cast<float>(substeps);

        for (int i = 0; i < substeps; ++i)
        {
            m_integration.Update(world, subDt);
            SyncDynamicAabbsToTransforms(world);

            m_events.clear();
            m_broadphaseAABBs.clear();
            m_broadphaseIds.clear();

            auto* aabbStorage = world.GetStorage<AABBComponent>();
            if (aabbStorage)
            {
                const auto& aabbs = aabbStorage->GetData();
                const auto& entities = aabbStorage->GetEntities();

                size_t count = aabbs.size();
                m_broadphaseAABBs.reserve(count);
                m_broadphaseIds.reserve(count);

                m_broadphaseAABBs.insert(m_broadphaseAABBs.end(), aabbs.begin(), aabbs.end());
                m_broadphaseIds.insert(m_broadphaseIds.end(), entities.begin(), entities.end());
            }

            auto* circleStorage = world.GetStorage<CircleColliderComponent>();
            auto* tfStorage = world.GetStorage<TransformComponent>();
            if (circleStorage && tfStorage)
            {
                const auto& circles = circleStorage->GetData();
                const auto& entities = circleStorage->GetEntities();
                const size_t count = circles.size();

                m_broadphaseAABBs.reserve(m_broadphaseAABBs.size() + count);
                m_broadphaseIds.reserve(m_broadphaseIds.size() + count);

                for (size_t j = 0; j < count; ++j)
                {
                    const ecs::EntityId id = entities[j];
                    if (aabbStorage && aabbStorage->Get(id))
                    {
                        continue;
                    }

                    auto* tf = tfStorage->Get(id);
                    if (!tf)
                    {
                        continue;
                    }

                    const auto& circle = circles[j];
                    const float radius = std::max(0.0f, circle.radius);
                    const float cx = tf->x + circle.offsetX;
                    const float cy = tf->y + circle.offsetY;
                    m_broadphaseAABBs.push_back({cx - radius, cy - radius, cx + radius, cy + radius});
                    m_broadphaseIds.push_back(id);
                }
            }

            if (!m_broadphaseAABBs.empty())
            {
                m_collision.Detect(m_broadphaseAABBs, m_broadphaseIds, m_events, m_jobSystem);
            }

            if (!m_events.empty()) {
                m_resolution.ResolvePosition(m_events, world, m_jobSystem);
            }

            m_constraints.Resolve(world, subDt);
            m_integration.UpdateVelocities(world, subDt);

            if (!m_events.empty()) {
                m_resolution.ResolveVelocity(m_events, world, m_jobSystem);
            }
        }
    }

    void PhysicsSystem::SetSettings(const PhysicsSettings& settings)
    {
        m_settings = settings;
        ApplySettings();
    }

    void PhysicsSystem::ApplySettings()
    {
        CollisionResolutionSystem::SolverSettings solver{};
        solver.positionIterations = m_settings.positionIterations;
        solver.velocityIterations = m_settings.velocityIterations;
        solver.penetrationSlop = m_settings.penetrationSlop;
        solver.correctionPercent = m_settings.correctionPercent;
        solver.maxCorrection = m_settings.maxPositionCorrection;
        m_resolution.SetSolverSettings(solver);
        m_constraints.SetIterationCount(m_settings.constraintIterations);
    }
}
