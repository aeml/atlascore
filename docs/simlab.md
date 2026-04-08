# Simulation Lab Module

The `simlab` module contains the scenario framework and a collection of scenarios used for benchmarks, determinism tests, and visualization demos.

## Core Components

-   **`IScenario`**: The interface for all scenarios.
-   `Setup(World&)`: Initializes the world with entities and systems.
-   `Update(World&, float)`: Scenario-specific update hook (engine steps `world.Update(dt)`).
-   `Render(World&, std::ostream&)`: Renders the current state to an output stream.
-   Headless app runs also emit `headless_metrics.csv` for per-frame state/timing metrics, `headless_summary.csv` for one-row run summaries (now including requested vs resolved scenario identity, fallback status, fixed dt, explicit bounded/unbounded frame-cap metadata, headless flag, run-config hash, run outcome fields, and termination reason before the aggregate counters/timings), and `headless_manifest.csv` for scenario/frame/path/timestamp/provenance indexing. The manifest also records per-artifact write status (`output_write_status`, `metrics_write_status`, `summary_write_status`) plus failure categories, alongside batch-index linkage/status (`batch_index_path`, `batch_index_append_status`, `batch_index_failure_category`), so sweep tooling can distinguish "run succeeded but summary export failed" or "run succeeded but ledger append failed" from actual simulation failures. It now also records the reporter’s own write status (`manifest_write_status`) and the fallback startup-failure artifact write status (`startup_failure_summary_write_status`, `startup_failure_manifest_write_status`) so export automation can see when the observability path itself degraded. It also records `exit_code` and `exit_classification`, so downstream tooling does not need to infer process outcome from shell behavior alone. Batch index failures are currently classified as either `batch_index_open_failed` or `batch_index_write_failed`; export failures are currently classified as `output_write_failed`, `metrics_write_failed`, `summary_write_failed`, `manifest_write_failed`, `startup_failure_summary_write_failed`, or `startup_failure_manifest_write_failed`. Current exit classifications are `success_exit` and `startup_failure_exit`. `--output-prefix=PATH_BASE` redirects all four artifacts to a caller-chosen path base for batch runs. `--batch-index=PATH.csv` appends the manifest row into a shared batch ledger for multi-run sweeps. If startup fails before normal artifact paths can be opened, AtlasCore emits fallback startup-failure summary/manifest files in the working directory instead of pretending the run never happened.
-   **`ScenarioRegistry`**: A singleton registry that manages available scenarios. It allows looking up scenarios by key and creating instances.
-   **`WorldHasher`**: A utility for generating a deterministic hash of live world state (transforms, rigid bodies, AABBs, circle colliders, joints). Used for verifying determinism across runs and for scenario-level regression tests.

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
