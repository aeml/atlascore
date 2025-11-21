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
            HashBytes(h, &b.vx, sizeof(b.vx));
            HashBytes(h, &b.vy, sizeof(b.vy));
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
}
