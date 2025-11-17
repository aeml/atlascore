# Simulation Lab Module

The `simlab` module contains scenarios used for benchmarks, determinism tests, and visualization demos.

- `IScenario`: interface with `Setup(World&)` and `Step(World&, float)`.
- `CreateDeterminismSmokeTest()`: falling-body demo with ASCII output.
- `CreateDeterminismHashScenario()`: runs two identical simulations side-by-side and hashes body state each step.
- `WorldHasher`: FNV-1a based utility producing stable 64-bit hashes from transforms and rigid bodies.

Future additions will include stress-test scenarios, collision determinism checks, and CSV export.
