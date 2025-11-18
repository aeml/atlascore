# Physics Module

The `physics` module defines physics-related components and systems.

- `TransformComponent`: position in 2D (x, y).
- `RigidBodyComponent`: velocity in 2D (vx, vy).
- `PhysicsIntegrationSystem`: integrates velocity into position applying a gravity constant (currently -9.81 m/s² along y). Supports parallel execution via `JobSystem`.

In the current skeleton, integration operates over local vectors of components inside the simulation scenario. Future work will route this through ECS-managed component storage.

## Collision (Stub)

Initial collision support introduces:

- `AABBComponent`: axis-aligned bounding box storing min/max coordinates.
- `CollisionSystem`: naive O(N^2) pairwise overlap checks producing `CollisionEvent` entries (indices of colliding boxes).

Determinism hashing now includes AABB data and collision event counts in dedicated tests. Future iterations will:

- Integrate AABBs into ECS component storage.
- Add broad-phase (spatial hash or sweep) before narrow AABB checks.
- Provide collision resolution (contact points, simple positional correction).
