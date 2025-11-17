#include "ecs/World.hpp"

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
