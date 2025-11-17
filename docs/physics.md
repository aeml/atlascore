# Physics Module

The `physics` module defines physics-related components and systems.

- `TransformComponent`: position in 2D (x, y).
- `RigidBodyComponent`: velocity in 2D (vx, vy).
- `PhysicsIntegrationSystem`: integrates velocity into position applying a gravity constant (currently -9.81 m/s² along y) for the demo.

In the current skeleton, integration operates over local vectors of components inside the simulation scenario. Future work will route this through ECS-managed component storage.
