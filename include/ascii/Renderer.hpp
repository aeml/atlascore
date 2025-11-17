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
