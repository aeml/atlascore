# ECS Module

The `ecs` module hosts the entity-component-system core.

- `World`: manages `EntityId`, systems, and component storages via a type-erased map keyed by `typeid(T).hash_code()`.
- `ISystem`: polymorphic system interface with `Update(World&, float)`.
- `ComponentStorage<T>`: simple map-based storage utility for components backing the world’s type-erased wrappers.

Adding a component:
```cpp
auto entity = world.CreateEntity();
auto& transform = world.AddComponent<physics::TransformComponent>(entity, physics::TransformComponent{0.f, 5.f});
auto* fetched = world.GetComponent<physics::TransformComponent>(entity);
```
This storage model favors simplicity; future optimizations may include contiguous arrays, archetypes, or chunked pools for cache locality.
