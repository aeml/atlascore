# Physics Module

The `physics` module defines physics-related components and systems.

- `TransformComponent`: position in 2D (x, y).
- `RigidBodyComponent`: velocity (vx, vy), mass, inverse mass, and restitution.
- `PhysicsIntegrationSystem`: integrates velocity into position applying a gravity constant (currently -9.81 m/s² along y). Supports parallel execution via `JobSystem`.

In the current skeleton, integration operates over local vectors of components inside the simulation scenario. Future work will route this through ECS-managed component storage.

## Collision

Collision support includes:

- `AABBComponent`: axis-aligned bounding box storing min/max coordinates.
- `CollisionSystem`: 
  - Uses a **Spatial Hash** broad-phase for efficient collision detection when body count > 100.
  - Supports parallel execution via `JobSystem`.
  - Falls back to O(N^2) for small datasets.
- `CollisionResolutionSystem`: resolves collisions using impulse-based response and positional correction.

Determinism hashing now includes AABB data and collision event counts in dedicated tests. Future iterations will:

- Integrate AABBs into ECS component storage.


