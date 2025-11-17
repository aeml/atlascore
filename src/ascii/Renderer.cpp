#include "ascii/Renderer.hpp"

namespace ascii
{
    Renderer::Renderer(std::ostream& out) noexcept
        : m_out(out)
    {
    }

    void Renderer::Render(const ecs::World& world) const
    {
        (void)world;
        m_out << "[ascii] Render frame (world stub)" << '\n';
    }

    void Renderer::RenderBodies(const std::vector<physics::TransformComponent>& transforms,
                                const std::vector<physics::RigidBodyComponent>& bodies) const
    {
        const std::size_t count = std::min(transforms.size(), bodies.size());
        m_out << "[ascii] Bodies:" << '\n';
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& t = transforms[i];
            const auto& b = bodies[i];
            m_out << "  #" << i << " x=" << t.x << " y=" << t.y
                  << " vx=" << b.vx << " vy=" << b.vy << '\n';
        }
    }
}
