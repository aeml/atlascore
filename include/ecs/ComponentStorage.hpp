#pragma once

#include <unordered_map>

#include "ecs/World.hpp"

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
