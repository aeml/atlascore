# Simulation Lab Module

The `simlab` module contains the scenario framework and a collection of scenarios used for benchmarks, determinism tests, and visualization demos.

## Core Components

-   **`IScenario`**: The interface for all scenarios.
-   `Setup(World&)`: Initializes the world with entities and systems.
-   `Update(World&, float)`: Scenario-specific update hook (engine steps `world.Update(dt)`).
-   `Render(World&, std::ostream&)`: Renders the current state to an output stream.
-   **`ScenarioRegistry`**: A singleton registry that manages available scenarios. It allows looking up scenarios by key and creating instances.
-   **`WorldHasher`**: A utility for generating a deterministic hash of the world state (transforms, rigid bodies, etc.). Used for verifying determinism across runs.

## Built-in Scenarios

The following scenarios are registered by default:

| Key | Description |
| :--- | :--- |
| `gravity` | **Planetary Gravity (N-Body)**: Simulates gravitational attraction between multiple bodies. |
| `wrecking` | **Wrecking Ball (Joints & Collisions)**: Demonstrates a wrecking ball attached via a distance joint smashing into a stack of boxes. |
| `fluid` | **Particle Fluid (High Entity Count)**: Simulates a fluid-like behavior using many small particles in an enclosed container. |

## Running Scenarios

Scenarios can be run via the command line:

```bash
./atlascore_app [scenario_key]
```

Or interactively by running the app without arguments.

## Adding a New Scenario

1.  Create a new class inheriting from `IScenario`.
2.  Implement `Setup`, `Update`, and `Render`.
3.  Register the scenario in `src/simlab/ScenarioRegistry.cpp` (or via a static registration helper).

```cpp
// Example registration
ScenarioRegistry::Register("my_scenario", "My custom scenario", &CreateMyScenario);
```
