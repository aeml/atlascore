# Simulation Lab Module

The `simlab` module contains scenarios used for benchmarks and determinism tests.

- `IScenario`: interface with `Setup(World&)` and `Step(World&, float)`.
- `CreateDeterminismSmokeTest()`: simple scenario used by `main`.
