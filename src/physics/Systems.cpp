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
        struct Contact
        {
            RigidBodyComponent* bA;
            RigidBodyComponent* bB;
            TransformComponent* tA;
            TransformComponent* tB;
            ecs::EntityId entityA;
            ecs::EntityId entityB;
            float leverA{0.0f};
            float leverB{0.0f};
            float nx;
            float ny;
            float pen;
            float restitution;
            float friction;
            float invMassSum;
        };

        constexpr float kContactEpsilon = 1e-6f;

        float EstimateLever(const CircleColliderComponent* circle, const AABBComponent* aabb)
        {
            if (circle)
            {
                return std::max(circle->radius, 0.0f);
            }
            if (aabb)
            {
                const float w = aabb->maxX - aabb->minX;
                const float h = aabb->maxY - aabb->minY;
                return 0.5f * std::sqrt(std::max(0.0f, w * w + h * h));
            }
            return 0.0f;
        }

        bool CircleCircleManifold(const TransformComponent& tA,
                                  const CircleColliderComponent& cA,
                                  const TransformComponent& tB,
                                  const CircleColliderComponent& cB,
                                  float& nx,
                                  float& ny,
                                  float& penetration)
        {
            const float ax = tA.x + cA.offsetX;
            const float ay = tA.y + cA.offsetY;
            const float bx = tB.x + cB.offsetX;
            const float by = tB.y + cB.offsetY;

            // Vector from A to B (Normal direction for solver)
            const float dx = bx - ax;
            const float dy = by - ay;
            const float radiusA = std::max(cA.radius, 0.0f);
            const float radiusB = std::max(cB.radius, 0.0f);
            const float radii = radiusA + radiusB;
            if (radii <= 0.0f) return false;

            const float distSq = dx * dx + dy * dy;
            if (distSq <= kContactEpsilon)
            {
                penetration = radii;
                // Arbitrary normal if centers coincide
                nx = 0.0f;
                ny = 1.0f;
                return true;
            }

            const float dist = std::sqrt(distSq);
            if (dist >= radii) return false;

            nx = dx / dist;
            ny = dy / dist;
            penetration = radii - dist;
            return penetration > 0.0f;
        }

        bool CircleAabbManifold(const TransformComponent& tCircle,
                                const CircleColliderComponent& circle,
                                const AABBComponent& box,
                                float& nx,
                                float& ny,
                                float& penetration)
        {
            const float cx = tCircle.x + circle.offsetX;
            const float cy = tCircle.y + circle.offsetY;
            const float radius = std::max(circle.radius, 0.0f);
            if (radius <= 0.0f) return false;

            const float closestX = std::clamp(cx, box.minX, box.maxX);
            const float closestY = std::clamp(cy, box.minY, box.maxY);
            
            // Vector from Circle to Box (Normal direction for solver)
            const float dx = closestX - cx;
            const float dy = closestY - cy;
            const float distSq = dx * dx + dy * dy;

            if (distSq > radius * radius + kContactEpsilon)
            {
                return false;
            }

            if (distSq > kContactEpsilon)
            {
                const float dist = std::sqrt(distSq);
                nx = dx / dist;
                ny = dy / dist;
                penetration = radius - dist;
                return penetration > 0.0f;
            }

            // Circle center inside the box. Push out along the closest face.
            const float left = cx - box.minX;
            const float right = box.maxX - cx;
            const float bottom = cy - box.minY;
            const float top = box.maxY - cy;
            float minDist = left;
            
            // Default to pushing Left (towards minX). Solver applies -Normal to A.
            // To push Left (-1,0), we need Normal = (1,0).
            nx = 1.0f;
            ny = 0.0f;

            if (right < minDist) { minDist = right; nx = -1.0f; ny = 0.0f; }
            if (bottom < minDist) { minDist = bottom; nx = 0.0f; ny = 1.0f; }
            if (top < minDist) { minDist = top; nx = 0.0f; ny = -1.0f; }

            // Penetration is radius + distance to edge (to clear the edge)
            penetration = radius + minDist;
            return true;
        }

        std::vector<Contact> GatherContacts(const std::vector<CollisionEvent>& events, ecs::World& world)
        {
            auto* tfStorage = world.GetStorage<TransformComponent>();
            auto* rbStorage = world.GetStorage<RigidBodyComponent>();
            auto* aabbStorage = world.GetStorage<AABBComponent>();
            auto* circleStorage = world.GetStorage<CircleColliderComponent>();

            std::vector<Contact> contacts;
            if (!tfStorage || !rbStorage) return contacts;

            contacts.reserve(events.size());

            for (const auto& event : events)
            {
                ecs::EntityId idA = event.entityA;
                ecs::EntityId idB = event.entityB;

                auto* bA = rbStorage->Get(idA);
                auto* bB = rbStorage->Get(idB);
                auto* tA = tfStorage->Get(idA);
                auto* tB = tfStorage->Get(idB);
                auto* aabbA = aabbStorage ? aabbStorage->Get(idA) : nullptr;
                auto* aabbB = aabbStorage ? aabbStorage->Get(idB) : nullptr;

                if (!bA || !bB || !tA || !tB) continue;

                float nx = event.normalX;
                float ny = event.normalY;
                float pen = event.penetration;

                auto* cA = circleStorage ? circleStorage->Get(idA) : nullptr;
                auto* cB = circleStorage ? circleStorage->Get(idB) : nullptr;

                if (cA && cB)
                {
                    if (!CircleCircleManifold(*tA, *cA, *tB, *cB, nx, ny, pen))
                    {
                        continue;
                    }
                }
                else if (cA && !cB)
                {
                    if (aabbB)
                    {
                        if (!CircleAabbManifold(*tA, *cA, *aabbB, nx, ny, pen))
                        {
                            continue;
                        }
                    }
                }
                else if (!cA && cB)
                {
                    if (aabbA)
                    {
                        if (!CircleAabbManifold(*tB, *cB, *aabbA, nx, ny, pen))
                        {
                            continue;
                        }
                        nx = -nx;
                        ny = -ny;
                    }
                }

                if (pen <= 0.0f) continue;

                Contact c;
                c.bA = bA;
                c.bB = bB;
                c.tA = tA;
                c.tB = tB;
                c.entityA = idA;
                c.entityB = idB;
                c.leverA = EstimateLever(cA, aabbA);
                c.leverB = EstimateLever(cB, aabbB);
                c.nx = nx;
                c.ny = ny;
                c.pen = pen;
                c.restitution = std::min(bA->restitution, bB->restitution);
                c.friction = std::sqrt(bA->friction * bA->friction + bB->friction * bB->friction);
                c.invMassSum = bA->invMass + bB->invMass;
                if (c.invMassSum == 0.0f) continue;

                contacts.push_back(c);
            }
            return contacts;
        }

        struct IslandBuilder
        {
            int Add(ecs::EntityId id)
            {
                auto it = indices.find(id);
                if (it != indices.end())
                {
                    return it->second;
                }
                const int idx = static_cast<int>(parent.size());
                indices.emplace(id, idx);
                parent.push_back(idx);
                rank.push_back(0);
                return idx;
            }

            void Unite(ecs::EntityId a, ecs::EntityId b)
            {
                const int ia = Add(a);
                const int ib = Add(b);
                int ra = Find(ia);
                int rb = Find(ib);
                if (ra == rb) return;
                if (rank[ra] < rank[rb]) std::swap(ra, rb);
                parent[rb] = ra;
                if (rank[ra] == rank[rb]) ++rank[ra];
            }

            int Root(ecs::EntityId id)
            {
                auto it = indices.find(id);
                if (it == indices.end()) return -1;
                return Find(it->second);
            }

        private:
            int Find(int idx)
            {
                if (parent[idx] != idx)
                {
                    parent[idx] = Find(parent[idx]);
                }
                return parent[idx];
            }

            std::unordered_map<ecs::EntityId, int> indices;
            std::vector<int> parent;
            std::vector<int> rank;
        };

        std::vector<std::vector<const Contact*>> BuildIslands(const std::vector<Contact>& contacts)
        {
            std::vector<std::vector<const Contact*>> islands;
            if (contacts.empty())
            {
                return islands;
            }

            IslandBuilder builder;
            for (const auto& c : contacts)
            {
                builder.Unite(c.entityA, c.entityB);
            }

            std::unordered_map<int, std::size_t> rootToIsland;
            islands.reserve(contacts.size());
            for (const auto& c : contacts)
            {
                const int root = builder.Root(c.entityA);
                if (root < 0) continue;
                auto [it, inserted] = rootToIsland.emplace(root, islands.size());
                std::size_t islandIndex;
                if (inserted)
                {
                    islands.emplace_back();
                    islandIndex = islands.size() - 1;
                }
                else
                {
                    islandIndex = it->second;
                }
                islands[islandIndex].push_back(&c);
            }
            return islands;
        }

        template <typename Fn>
        void ExecuteIslands(const std::vector<std::vector<const Contact*>>& islands,
                            jobs::JobSystem* jobSystem,
                            Fn&& fn)
        {
            if (islands.empty())
            {
                return;
            }

            if (!jobSystem || islands.size() < 2)
            {
                for (const auto& island : islands)
                {
                    fn(island);
                }
                return;
            }

            const std::size_t workerCount = std::max<std::size_t>(1, jobSystem->WorkerCount());
            const std::size_t batch = std::max<std::size_t>(1, islands.size() / (workerCount * 2));
            auto handles = jobSystem->Dispatch(islands.size(), batch, [&](std::size_t start, std::size_t end)
            {
                for (std::size_t i = start; i < end; ++i)
                {
                    fn(islands[i]);
                }
            });
            jobSystem->Wait(handles);
        }
    }

    void CollisionResolutionSystem::ResolvePosition(const std::vector<CollisionEvent>& events, ecs::World& world, jobs::JobSystem* jobSystem) const
    {
        auto contacts = GatherContacts(events, world);
        if (contacts.empty())
        {
            return;
        }

        const int positionIterations = std::max(1, m_settings.positionIterations);
        const float percent = m_settings.correctionPercent;
        const float slop = m_settings.penetrationSlop;
        const float maxCorrection = m_settings.maxCorrection;
        auto islands = BuildIslands(contacts);

        auto solveIsland = [&](const std::vector<const Contact*>& islandContacts)
        {
            for (int i = 0; i < positionIterations; ++i)
            {
                for (const Contact* contactPtr : islandContacts)
                {
                    const auto& c = *contactPtr;
                    float correction = std::max(c.pen - slop, 0.0f) / c.invMassSum * percent;
                    if (correction > maxCorrection) correction = maxCorrection;

                    float cx = correction * c.nx;
                    float cy = correction * c.ny;

                    c.tA->x -= cx * c.bA->invMass;
                    c.tA->y -= cy * c.bA->invMass;
                    c.tB->x += cx * c.bB->invMass;
                    c.tB->y += cy * c.bB->invMass;
                }
            }
        };

        ExecuteIslands(islands, jobSystem, solveIsland);
    }

    void CollisionResolutionSystem::ResolveVelocity(const std::vector<CollisionEvent>& events, ecs::World& world, jobs::JobSystem* jobSystem) const
    {
        auto contacts = GatherContacts(events, world);
        if (contacts.empty())
        {
            return;
        }

        const int velocityIterations = std::max(1, m_settings.velocityIterations);
        auto islands = BuildIslands(contacts);

        auto solveIsland = [&](const std::vector<const Contact*>& islandContacts)
        {
            for (int i = 0; i < velocityIterations; ++i)
            {
                for (const Contact* contactPtr : islandContacts)
                {
                    const auto& c = *contactPtr;
                    float rvx = c.bB->vx - c.bA->vx;
                    float rvy = c.bB->vy - c.bA->vy;

                    float velAlongNormal = rvx * c.nx + rvy * c.ny;

                    float j = 0.0f;
                    if (velAlongNormal < 0)
                    {
                        j = -(1 + c.restitution) * velAlongNormal;
                        j /= c.invMassSum;

                        float impulseX = j * c.nx;
                        float impulseY = j * c.ny;

                        c.bA->vx -= impulseX * c.bA->invMass;
                        c.bA->vy -= impulseY * c.bA->invMass;
                        c.bB->vx += impulseX * c.bB->invMass;
                        c.bB->vy += impulseY * c.bB->invMass;
                    }

                    if (c.pen > -0.05f)
                    {
                        rvx = c.bB->vx - c.bA->vx;
                        rvy = c.bB->vy - c.bA->vy;

                        float tx = -c.ny;
                        float ty = c.nx;
                        float velAlongTangent = rvx * tx + rvy * ty;

                        float jt = -velAlongTangent;
                        jt /= c.invMassSum;

                        float maxJt = 0.0f;
                        if (j > 0) {
                            maxJt = c.friction * j;
                        } else {
                            maxJt = c.friction * 0.1f;
                        }

                        if (std::abs(jt) > maxJt)
                        {
                            jt = (jt > 0) ? maxJt : -maxJt;
                        }

                        float frictionImpulseX = jt * tx;
                        float frictionImpulseY = jt * ty;

                        c.bA->vx -= frictionImpulseX * c.bA->invMass;
                        c.bA->vy -= frictionImpulseY * c.bA->invMass;
                        c.bB->vx += frictionImpulseX * c.bB->invMass;
                        c.bB->vy += frictionImpulseY * c.bB->invMass;

                        if (c.leverA > 0.0f && c.bA->invInertia > 0.0f)
                        {
                            c.bA->angularVelocity -= jt * c.leverA * c.bA->invInertia;
                        }
                        if (c.leverB > 0.0f && c.bB->invInertia > 0.0f)
                        {
                            c.bB->angularVelocity += jt * c.leverB * c.bB->invInertia;
                        }
                    }
                }
            }
        };

        ExecuteIslands(islands, jobSystem, solveIsland);
    }

    void CollisionResolutionSystem::Resolve(const std::vector<CollisionEvent>& events, ecs::World& world) const
    {
        ResolvePosition(events, world, nullptr);
        ResolveVelocity(events, world, nullptr);
    }

    void CollisionResolutionSystem::Resolve(const std::vector<CollisionEvent>& events,
                                            std::vector<TransformComponent>& transforms,
                                            std::vector<RigidBodyComponent>& bodies) const
    {
        for (const auto& event : events)
        {
            // Legacy/Test support: assume entityId corresponds to index in the provided vectors
            size_t idxA = static_cast<size_t>(event.entityA);
            size_t idxB = static_cast<size_t>(event.entityB);

            if (idxA >= transforms.size() || idxB >= transforms.size()) continue;

            auto& tA = transforms[idxA];
            auto& bA = bodies[idxA];
            auto& tB = transforms[idxB];
            auto& bB = bodies[idxB];

            // Relative velocity
            float rvx = bB.vx - bA.vx;
            float rvy = bB.vy - bA.vy;

            // Velocity along normal
            float velAlongNormal = rvx * event.normalX + rvy * event.normalY;

            // Do not resolve if velocities are separating
            if (velAlongNormal > 0)
                continue;

            // Restitution (min of both)
            float e = std::min(bA.restitution, bB.restitution);

            // Impulse scalar
            float j = -(1 + e) * velAlongNormal;
            j /= (bA.invMass + bB.invMass);

            // Apply impulse
            float impulseX = j * event.normalX;
            float impulseY = j * event.normalY;

            bA.vx -= impulseX * bA.invMass;
            bA.vy -= impulseY * bA.invMass;
            bB.vx += impulseX * bB.invMass;
            bB.vy += impulseY * bB.invMass;

            // Positional correction (Linear Projection)
            const float percent = 0.2f;
            const float slop = 0.01f;
            float correction = std::max(event.penetration - slop, 0.0f) / (bA.invMass + bB.invMass) * percent;
            
            float cx = correction * event.normalX;
            float cy = correction * event.normalY;

            tA.x -= cx * bA.invMass;
            tA.y -= cy * bA.invMass;
            tB.x += cx * bB.invMass;
            tB.y += cy * bB.invMass;
        }
    }

    void ConstraintResolutionSystem::Resolve(ecs::World& world, float dt) const
    {
        auto* jointStorage = world.GetStorage<DistanceJointComponent>();
        auto* tfStorage = world.GetStorage<TransformComponent>();
        auto* rbStorage = world.GetStorage<RigidBodyComponent>();

        if (!jointStorage || !tfStorage || !rbStorage) return;

        const auto& joints = jointStorage->GetData();
        
        // Pre-resolve pointers
        struct Constraint
        {
            TransformComponent* tA;
            TransformComponent* tB;
            RigidBodyComponent* bA;
            RigidBodyComponent* bB;
            float targetDistance;
            float invMassSum;
            float compliance;
        };

        std::vector<Constraint> constraints;
        constraints.reserve(joints.size());

        for (const auto& joint : joints)
        {
            auto* tA = tfStorage->Get(joint.entityA);
            auto* tB = tfStorage->Get(joint.entityB);
            auto* bA = rbStorage->Get(joint.entityA);
            auto* bB = rbStorage->Get(joint.entityB);

            if (!tA || !tB || !bA || !bB) continue;
            float invMassSum = bA->invMass + bB->invMass;
            if (invMassSum == 0.0f) continue;

            constraints.push_back({tA, tB, bA, bB, joint.targetDistance, invMassSum, joint.compliance});
        }

        const int iterations = std::max(1, m_iterations);
        const float dtSafe = std::max(dt, 1e-4f);
        for (int iter = 0; iter < iterations; ++iter)
        {
            for (const auto& c : constraints)
            {
                float dx = c.tB->x - c.tA->x;
                float dy = c.tB->y - c.tA->y;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist < 0.0001f) continue;

                float diff = dist - c.targetDistance;
                float complianceTerm = (c.compliance > 0.0f) ? (c.compliance / (dtSafe * dtSafe)) : 0.0f;
                float denom = c.invMassSum + complianceTerm;
                if (denom <= 0.0f) continue;
                float correction = diff / denom;

                float px = (dx / dist) * correction;
                float py = (dy / dist) * correction;

                c.tA->x += px * c.bA->invMass;
                c.tA->y += py * c.bA->invMass;
                c.tB->x -= px * c.bB->invMass;
                c.tB->y -= py * c.bB->invMass;
            }
        }
    }

}
