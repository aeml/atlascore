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
