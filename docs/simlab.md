# Simulation Lab Module

The `simlab` module contains the scenario framework and a collection of scenarios used for benchmarks, determinism tests, and visualization demos.

## Core Components

-   **`IScenario`**: The interface for all scenarios.
    -   `Setup(World&)`: Initializes the world with entities and systems.
    -   `Step(World&, float)`: Advances the simulation by a fixed timestep.
    -   `Render(World&)`: Renders the current state (optional).
-   **`ScenarioRegistry`**: A singleton registry that manages available scenarios. It allows looking up scenarios by key and creating instances.
-   **`WorldHasher`**: A utility for generating a deterministic hash of the world state (transforms, rigid bodies, etc.). Used for verifying determinism across runs.

## Built-in Scenarios

The following scenarios are registered by default:

| Key | Description |
| :--- | :--- |
| `smoke` | **ECS falling bodies smoke test**: A simple test with falling bodies to verify basic physics and ECS functionality. |
| `hash` | **Determinism hash dual-run scenario**: Runs two identical simulations side-by-side and compares their state hashes at every step to ensure determinism. |
| `stack` | **Pyramid stacking test**: Tests the stability of the physics solver with a stack of boxes. |
| `pendulum` | **Pendulum chain test**: Demonstrates distance joints and constraints. |
| `cloth` | **Cloth simulation test**: Simulates a piece of cloth using a grid of particles connected by distance joints. |
| `ball_showcase` | **Ball collision showcase**: A visual demo of various ball collisions. |
| `stress` | **Stress Test (2000 particles)**: A performance benchmark with a large number of particles. |
| `text_patterns` | **Text renderer patterns**: Demonstrates the capabilities of the ASCII renderer with various patterns. |

## Running Scenarios

Scenarios can be run via the command line:

```bash
./atlascore_app [scenario_key]
```

Or interactively by running the app without arguments.

## Adding a New Scenario

1.  Create a new class inheriting from `IScenario`.
2.  Implement `Setup`, `Step`, and optionally `Render`.
3.  Register the scenario in `src/simlab/ScenarioRegistry.cpp` (or via a static registration helper).

```cpp
// Example registration
ScenarioRegistry::Register("my_scenario", "My custom scenario", &CreateMyScenario);
```
