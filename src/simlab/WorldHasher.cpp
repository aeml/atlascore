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

#include "simlab/WorldHasher.hpp"

#include "ecs/World.hpp"

#include <algorithm>
#include <cstring>

namespace simlab
{

    void WorldHasher::HashBytes(std::uint64_t& h, const void* data, std::size_t len) const noexcept
    {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < len; ++i)
        {
            h ^= static_cast<std::uint64_t>(bytes[i]);
            h *= kPrime;
        }
    }

    std::uint64_t WorldHasher::HashBodies(const std::vector<physics::TransformComponent>& transforms,
                                          const std::vector<physics::RigidBodyComponent>& bodies) const noexcept
    {
        std::uint64_t h = kOffset;
        const std::size_t count = std::min(transforms.size(), bodies.size());
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& t = transforms[i];
            const auto& b = bodies[i];
            HashBytes(h, &t.x, sizeof(t.x));
            HashBytes(h, &t.y, sizeof(t.y));
            HashBytes(h, &t.rotation, sizeof(t.rotation));
            HashBytes(h, &b.vx, sizeof(b.vx));
            HashBytes(h, &b.vy, sizeof(b.vy));
            HashBytes(h, &b.angularVelocity, sizeof(b.angularVelocity));
            HashBytes(h, &b.mass, sizeof(b.mass));
            HashBytes(h, &b.invMass, sizeof(b.invMass));
            HashBytes(h, &b.inertia, sizeof(b.inertia));
            HashBytes(h, &b.invInertia, sizeof(b.invInertia));
            HashBytes(h, &b.restitution, sizeof(b.restitution));
            HashBytes(h, &b.friction, sizeof(b.friction));
        }
        return h;
    }

    std::uint64_t WorldHasher::HashAABBs(const std::vector<physics::AABBComponent>& aabbs) const noexcept
    {
        std::uint64_t h = kOffset;
        for (const auto& box : aabbs)
        {
            HashBytes(h, &box.minX, sizeof(box.minX));
            HashBytes(h, &box.minY, sizeof(box.minY));
            HashBytes(h, &box.maxX, sizeof(box.maxX));
            HashBytes(h, &box.maxY, sizeof(box.maxY));
        }
        return h;
    }

    std::uint64_t WorldHasher::HashWorld(const ecs::World& world) const noexcept
    {
        std::uint64_t h = kOffset;

        if (const auto* transforms = world.GetStorage<physics::TransformComponent>())
        {
            const auto& entities = transforms->GetEntities();
            const auto& data = transforms->GetData();
            for (std::size_t i = 0; i < data.size(); ++i)
            {
                HashBytes(h, &entities[i], sizeof(entities[i]));
                HashBytes(h, &data[i].x, sizeof(data[i].x));
                HashBytes(h, &data[i].y, sizeof(data[i].y));
                HashBytes(h, &data[i].rotation, sizeof(data[i].rotation));
            }
        }

        if (const auto* bodies = world.GetStorage<physics::RigidBodyComponent>())
        {
            const auto& entities = bodies->GetEntities();
            const auto& data = bodies->GetData();
            for (std::size_t i = 0; i < data.size(); ++i)
            {
                const auto& body = data[i];
                HashBytes(h, &entities[i], sizeof(entities[i]));
                HashBytes(h, &body.vx, sizeof(body.vx));
                HashBytes(h, &body.vy, sizeof(body.vy));
                HashBytes(h, &body.lastX, sizeof(body.lastX));
                HashBytes(h, &body.lastY, sizeof(body.lastY));
                HashBytes(h, &body.lastAngle, sizeof(body.lastAngle));
                HashBytes(h, &body.mass, sizeof(body.mass));
                HashBytes(h, &body.invMass, sizeof(body.invMass));
                HashBytes(h, &body.inertia, sizeof(body.inertia));
                HashBytes(h, &body.invInertia, sizeof(body.invInertia));
                HashBytes(h, &body.restitution, sizeof(body.restitution));
                HashBytes(h, &body.friction, sizeof(body.friction));
                HashBytes(h, &body.angularVelocity, sizeof(body.angularVelocity));
                HashBytes(h, &body.torque, sizeof(body.torque));
                HashBytes(h, &body.angularFriction, sizeof(body.angularFriction));
                HashBytes(h, &body.angularDrag, sizeof(body.angularDrag));
            }
        }

        if (const auto* aabbs = world.GetStorage<physics::AABBComponent>())
        {
            const auto& entities = aabbs->GetEntities();
            const auto& data = aabbs->GetData();
            for (std::size_t i = 0; i < data.size(); ++i)
            {
                HashBytes(h, &entities[i], sizeof(entities[i]));
                HashBytes(h, &data[i].minX, sizeof(data[i].minX));
                HashBytes(h, &data[i].minY, sizeof(data[i].minY));
                HashBytes(h, &data[i].maxX, sizeof(data[i].maxX));
                HashBytes(h, &data[i].maxY, sizeof(data[i].maxY));
            }
        }

        if (const auto* circles = world.GetStorage<physics::CircleColliderComponent>())
        {
            const auto& entities = circles->GetEntities();
            const auto& data = circles->GetData();
            for (std::size_t i = 0; i < data.size(); ++i)
            {
                HashBytes(h, &entities[i], sizeof(entities[i]));
                HashBytes(h, &data[i].radius, sizeof(data[i].radius));
                HashBytes(h, &data[i].offsetX, sizeof(data[i].offsetX));
                HashBytes(h, &data[i].offsetY, sizeof(data[i].offsetY));
            }
        }

        if (const auto* joints = world.GetStorage<physics::DistanceJointComponent>())
        {
            const auto& entities = joints->GetEntities();
            const auto& data = joints->GetData();
            for (std::size_t i = 0; i < data.size(); ++i)
            {
                HashBytes(h, &entities[i], sizeof(entities[i]));
                HashBytes(h, &data[i].entityA, sizeof(data[i].entityA));
                HashBytes(h, &data[i].entityB, sizeof(data[i].entityB));
                HashBytes(h, &data[i].targetDistance, sizeof(data[i].targetDistance));
                HashBytes(h, &data[i].compliance, sizeof(data[i].compliance));
            }
        }

        return h;
    }
}
