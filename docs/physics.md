# Physics Module

AtlasCore's `physics` module provides a deterministic 2D rigid body pipeline with optional constraint solving and collision resolution. All systems can operate over ECS component storage and may parallelize work via the `JobSystem` where appropriate.

## Components

| Component | Purpose |
|-----------|---------|
| `TransformComponent` | Position (x,y) + rotation for entities |
| `RigidBodyComponent` | Linear & angular velocity, mass, restitution, friction, inertia values, and PBD backup positions |
| `EnvironmentForces`  | Global gravity, wind, drag coefficients |
| `DistanceJointComponent` | Soft/rigid distance constraint between two entities (with compliance) |
| `AABBComponent` | Axis-aligned bounds used for broad-phase collision tests |
| `CircleColliderComponent` | Simple circular collider shape + offset |

Helper inertia configuration functions (`ConfigureCircleInertia`, `ConfigureBoxInertia`) populate inertia / inverse inertia consistently.

## Systems

| System | Role |
|--------|------|
| `PhysicsIntegrationSystem` | Applies environment forces; integrates velocities & transforms; optional parallel velocity update |
| `CollisionSystem` | Broad-phase AABB overlap detection producing `CollisionEvent` list (parallelizable) |
| `CollisionResolutionSystem` | Impulse-based velocity + positional correction; configurable solver iterations |
| `ConstraintResolutionSystem` | Resolves joints (`DistanceJointComponent`) over multiple iterations |
| `PhysicsSystem` | Orchestrator: integration → collision detect → constraint solve → collision resolve (position/velocity phases) |

`PhysicsSettings` allow tuning substeps and iteration counts. Separation of position vs. velocity iterations aids stability while maintaining deterministic ordering.

## Determinism

Physics determinism is ensured through:
* Fixed timestep (`FixedTimestepLoop`) feeding consistent `dt`.
* Stable iteration order over component storage.
* Atomic-free collision event accumulation (single-thread append or controlled parallel aggregation) to keep ordering reproducible.
* World state hashing that includes transforms, rigid body properties, AABB bounds, and collision counts.

## Parallelization

When a `JobSystem` is attached, integration and resolution phases may process batches of components concurrently. Work partitioning preserves deterministic final results by aggregating impulses in a controlled sequence.

## Future Work

Planned enhancements:
* Additional collider shapes (oriented boxes, segments).
* Narrow-phase refinement & contact caching.
* More joint types (angle limits, springs) + constraint warm starting.
* Profiling metrics surfaced via ASCII HUD.

## Summary

The current implementation balances clarity with determinism, offering a foundation for extending collision sophistication and constraint variety while retaining reproducible simulation outcomes.



