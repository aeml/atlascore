#pragma once

#include <unordered_map>

// Forward declaration of EntityId to avoid circular include with World.hpp.
#include <cstdint>
namespace ecs { using EntityId = std::uint32_t; }

namespace ecs
{
    template <typename TComponent>
    class ComponentStorage
    {
    public:
        TComponent& Add(EntityId id, const TComponent& component)
        {
            return m_components.emplace(id, component).first->second;
        }

        TComponent* Get(EntityId id)
        {
            auto it = m_components.find(id);
            return it != m_components.end() ? &it->second : nullptr;
        }

    private:
        std::unordered_map<EntityId, TComponent> m_components;
    };
}
