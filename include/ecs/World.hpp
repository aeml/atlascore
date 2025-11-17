#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <typeinfo>
#include <utility>

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
        TComponent& AddComponent(EntityId id, Args&&... args)
        {
            const std::size_t key = typeid(TComponent).hash_code();
            auto it = m_componentStores.find(key);
            if (it == m_componentStores.end())
            {
                auto wrapper = std::make_unique<StorageWrapper<TComponent>>();
                it = m_componentStores.emplace(key, std::move(wrapper)).first;
            }
            auto* storage = static_cast<StorageWrapper<TComponent>*>(it->second.get());
            TComponent value(std::forward<Args>(args)...);
            return storage->storage.Add(id, value);
        }

        template <typename TComponent>
        TComponent* GetComponent(EntityId id)
        {
            const std::size_t key = typeid(TComponent).hash_code();
            auto it = m_componentStores.find(key);
            if (it == m_componentStores.end())
            {
                return nullptr;
            }
            auto* storage = static_cast<StorageWrapper<TComponent>*>(it->second.get());
            return storage->storage.Get(id);
        }

        void AddSystem(std::unique_ptr<ISystem> system);
        void Update(float dt);

    private:
        EntityId                                      m_nextEntity{1};
        std::vector<EntityId>                         m_entities;
        std::vector<std::unique_ptr<ISystem>>         m_systems;
        struct IStorage
        {
            virtual ~IStorage() = default;
        };
        template <typename T>
        struct StorageWrapper : IStorage
        {
            ComponentStorage<T> storage;
        };
        std::unordered_map<std::size_t, std::unique_ptr<IStorage>> m_componentStores;
    };

    class ISystem
    {
    public:
        virtual ~ISystem() = default;
        virtual void Update(World& world, float dt) = 0;
    };
}
