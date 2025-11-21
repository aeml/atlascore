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
