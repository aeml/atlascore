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

#include "ecs/World.hpp"
#include <algorithm>

namespace ecs
{
    EntityId World::CreateEntity()
    {
        const EntityId id = m_nextEntity++;
        m_entities.push_back(id);
        return id;
    }

    void World::DestroyEntity(EntityId id)
    {
        m_entities.erase(
            std::remove(m_entities.begin(), m_entities.end(), id),
            m_entities.end());

        for (auto& [typeKey, storage] : m_componentStores)
        {
            (void)typeKey;
            if (storage)
            {
                storage->RemoveEntity(id);
            }
        }
    }

    void World::AddSystem(std::unique_ptr<ISystem> system)
    {
        if (system)
        {
            m_systems.emplace_back(std::move(system));
        }
    }

    void World::Update(float dt)
    {
        for (auto& system : m_systems)
        {
            system->Update(*this, dt);
        }
    }
}
