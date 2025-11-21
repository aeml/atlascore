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

#include <vector>
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
            if (m_entityToDense.find(id) != m_entityToDense.end())
            {
                // Update existing
                size_t index = m_entityToDense[id];
                m_data[index] = component;
                return m_data[index];
            }

            size_t index = m_data.size();
            m_data.push_back(component);
            m_denseToEntity.push_back(id);
            m_entityToDense[id] = index;
            return m_data.back();
        }

        TComponent* Get(EntityId id)
        {
            auto it = m_entityToDense.find(id);
            return it != m_entityToDense.end() ? &m_data[it->second] : nullptr;
        }

        template <typename Fn>
        void ForEach(Fn&& fn)
        {
            for (size_t i = 0; i < m_data.size(); ++i)
            {
                fn(m_denseToEntity[i], m_data[i]);
            }
        }

        // Direct access for systems (e.g. JobSystem)
        std::vector<TComponent>& GetData() { return m_data; }
        const std::vector<TComponent>& GetData() const { return m_data; }
        const std::vector<EntityId>& GetEntities() const { return m_denseToEntity; }
        size_t Size() const { return m_data.size(); }

    private:
        std::vector<TComponent> m_data;
        std::vector<EntityId>   m_denseToEntity;
        std::unordered_map<EntityId, size_t> m_entityToDense;
    };
}
