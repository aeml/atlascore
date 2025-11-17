# ECS Module

The `ecs` module hosts the entity-component-system core.

- `World`: manages `EntityId`, systems, and (future) component storages.
- `ISystem`: polymorphic system interface with `Update(World&, float)`.
- `ComponentStorage<T>`: simple map-based storage utility for components.
