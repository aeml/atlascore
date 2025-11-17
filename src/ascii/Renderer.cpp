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
        m_out << "[ascii] Render frame (stub)" << '\n';
    }
}
