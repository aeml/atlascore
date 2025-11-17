#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include <cstdint>

namespace ecs
{
    using EntityId = std::uint32_t;

    class ISystem;

    class World
    {
    public:
        World() = default;

        EntityId CreateEntity();
        void     DestroyEntity(EntityId id);

        template <typename TComponent, typename... Args>
        TComponent& AddComponent(EntityId id, Args&&... args);

        template <typename TComponent>
        TComponent* GetComponent(EntityId id);

        void AddSystem(std::unique_ptr<ISystem> system);
        void Update(float dt);

    private:
        EntityId                                      m_nextEntity{1};
        std::vector<EntityId>                         m_entities;
        std::vector<std::unique_ptr<ISystem>>         m_systems;
        std::unordered_map<std::size_t, void*>        m_componentStores; // placeholder
    };

    class ISystem
    {
    public:
        virtual ~ISystem() = default;
        virtual void Update(World& world, float dt) = 0;
    };
}
