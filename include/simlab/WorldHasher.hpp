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

#include <cstdint>
#include <vector>

#include "physics/Components.hpp"

namespace simlab
{
    // Simple FNV-1a 64-bit hashing over body transforms and velocities.
    class WorldHasher
    {
    public:
        std::uint64_t HashBodies(const std::vector<physics::TransformComponent>& transforms,
                                 const std::vector<physics::RigidBodyComponent>& bodies) const noexcept;
        std::uint64_t HashAABBs(const std::vector<physics::AABBComponent>& aabbs) const noexcept;
        std::uint64_t Combine(std::uint64_t h1, std::uint64_t h2) const noexcept { return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1<<6) + (h1>>2)); }
    private:
        static constexpr std::uint64_t kOffset = 1469598103934665603ull;
        static constexpr std::uint64_t kPrime  = 1099511628211ull;
        void HashBytes(std::uint64_t& h, const void* data, std::size_t len) const noexcept;
    };
}
