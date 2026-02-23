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

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <utility>
#include <tuple>
#include <optional>

#include "ecs/ComponentStorage.hpp"

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
            const std::type_index key{typeid(TComponent)};
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
            const std::type_index key{typeid(TComponent)};
            auto it = m_componentStores.find(key);
            if (it == m_componentStores.end())
            {
                return nullptr;
            }
            auto* storage = static_cast<StorageWrapper<TComponent>*>(it->second.get());
            return storage->storage.Get(id);
        }

        template <typename TComponent>
        ComponentStorage<TComponent>* GetStorage()
        {
            const std::type_index key{typeid(TComponent)};
            auto it = m_componentStores.find(key);
            if (it == m_componentStores.end())
            {
                return nullptr;
            }
            auto* storage = static_cast<StorageWrapper<TComponent>*>(it->second.get());
            return &storage->storage;
        }

        void AddSystem(std::unique_ptr<ISystem> system);
        void Update(float dt);

        template <typename TComponent, typename Fn>
        void ForEach(Fn&& fn)
        {
            const std::type_index key{typeid(TComponent)};
            auto it = m_componentStores.find(key);
            if (it == m_componentStores.end())
            {
                return;
            }
            auto* storage = static_cast<StorageWrapper<TComponent>*>(it->second.get());
            storage->storage.ForEach(std::forward<Fn>(fn));
        }

        // Multi-component View
        // Usage: world.View<Transform, Velocity>([&](EntityId id, Transform& t, Velocity& v) { ... });
        template <typename T1, typename T2, typename... TRest, typename Fn>
        void View(Fn&& fn)
        {
            // 1. Find all storages
            ComponentStorage<T1>* s1 = GetStorage<T1>();
            ComponentStorage<T2>* s2 = GetStorage<T2>();
            std::tuple<ComponentStorage<TRest>*...> restStores = std::make_tuple(GetStorage<TRest>()...);

            if (!s1 || !s2 || ((!std::get<ComponentStorage<TRest>*>(restStores)) || ...)) return;

            // 2. Find the smallest storage to iterate
            size_t minSize = s1->Size();
            int smallestIndex = 0; // 0 for s1, 1 for s2, 2+ for TRest

            if (s2->Size() < minSize) {
                minSize = s2->Size();
                smallestIndex = 1;
            }

            // Check rest (compile-time loop would be better, but runtime is fine for setup)
            int idx = 2;
            auto checkSize = [&](auto* s) {
                if (s->Size() < minSize) {
                    minSize = s->Size();
                    smallestIndex = idx;
                }
                idx++;
            };
            std::apply([&](auto*... s) { (checkSize(s), ...); }, restStores);

            // 3. Iterate the smallest
            auto process = [&](EntityId id) {
                // We have the ID, now fetch all components.
                // We know they exist in the "smallest" storage, but we must check existence in others.
                // However, since we are iterating the intersection, we need to check if 'id' exists in ALL others.
                
                T1* c1 = s1->Get(id);
                T2* c2 = s2->Get(id);
                if (c1 && c2) {
                    if (auto tuple = GetComponentsTuple<TRest...>(id)) {
                         std::apply([&](auto&... args) {
                            fn(id, *c1, *c2, args...);
                        }, *tuple);
                    }
                }
            };

            if (smallestIndex == 0) s1->ForEach([&](EntityId id, T1&) { process(id); });
            else if (smallestIndex == 1) s2->ForEach([&](EntityId id, T2&) { process(id); });
            else {
                // Iterate the correct storage from tuple
                // This is tricky with runtime index. For simplicity in this version, 
                // we'll just default to s1 if it's not s2, or improve logic later.
                // To keep it simple and robust for now:
                s1->ForEach([&](EntityId id, T1&) { process(id); });
            }
        }

    private:
        // Helper to get a tuple of pointers to components, or nullopt if any are missing
        template <typename... Ts>
        std::optional<std::tuple<Ts&...>> GetComponentsTuple(EntityId id)
        {
            std::tuple<Ts*...> ptrs = std::make_tuple(GetComponent<Ts>(id)...);
            bool allFound = ((std::get<Ts*>(ptrs) != nullptr) && ...);
            if (!allFound) return std::nullopt;
            
            return std::tuple<Ts&...>(*std::get<Ts*>(ptrs)...);
        }

        EntityId                                      m_nextEntity{1};
        std::vector<EntityId>                         m_entities;
        std::vector<std::unique_ptr<ISystem>>         m_systems;
        struct IStorage
        {
            virtual ~IStorage() = default;
            virtual void RemoveEntity(EntityId id) = 0;
        };
        template <typename T>
        struct StorageWrapper : IStorage
        {
            ComponentStorage<T> storage;

            void RemoveEntity(EntityId id) override
            {
                storage.Remove(id);
            }
        };
        std::unordered_map<std::type_index, std::unique_ptr<IStorage>> m_componentStores;
    };

    class ISystem
    {
    public:
        virtual ~ISystem() = default;
        virtual void Update(World& world, float dt) = 0;
    };
}
