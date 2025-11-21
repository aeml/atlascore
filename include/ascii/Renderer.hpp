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

#include <ostream>
#include <vector>

#include "ecs/World.hpp"
#include "physics/Components.hpp"

namespace ascii
{
    class Renderer
    {
    public:
        explicit Renderer(std::ostream& out) noexcept;

        void Render(const ecs::World& world) const; // Future ECS usage.

        void RenderBodies(const std::vector<physics::TransformComponent>& transforms,
                          const std::vector<physics::RigidBodyComponent>& bodies) const;

    private:
        std::ostream& m_out;
    };
}
