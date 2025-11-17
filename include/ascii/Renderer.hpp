#pragma once

#include <ostream>

#include "ecs/World.hpp"

namespace ascii
{
    class Renderer
    {
    public:
        explicit Renderer(std::ostream& out) noexcept;

        void Render(const ecs::World& world) const;

    private:
        std::ostream& m_out;
    };
}
