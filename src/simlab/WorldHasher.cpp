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
}
