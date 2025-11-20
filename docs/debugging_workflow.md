# Debugging and Verification Workflow

This document outlines the strategy for debugging and verifying the AtlasCore engine, ensuring that backend systems (Physics, ECS, Jobs) are correct before addressing rendering issues.

## Separation of Concerns

The `IScenario` interface has been refactored to separate simulation logic from rendering logic:

-   `Update(ecs::World& world, float dt)`: Contains all simulation logic (physics integration, game logic, spawning). This must be deterministic and independent of the renderer.
-   `Render(ecs::World& world)`: Contains all visualization logic (drawing to `TextRenderer`). This is only called when not in headless mode.

## Verification Strategy

### 1. Backend Verification (Headless)

To verify that the physics engine, ECS, and job system are working correctly *without* visual interference:

1.  **Run Unit Tests**:
    ```bash
    cd build
    ctest --output-on-failure
    ```
    These tests cover individual components and integration scenarios.

2.  **Run Headless Scenarios**:
    You can run the main application in headless mode. This runs the `Update` loop but skips `Render`.
    ```bash
    ./atlascore_app --headless [scenario_name]
    ```
    Use this to check for crashes, memory leaks (with Valgrind/ASan), or performance issues in the simulation loop.

3.  **Simlab Scenario Tests**:
    The test suite `atlascore_simlab_scenarios_tests` runs scenarios in headless mode and asserts that:
    -   Dynamic bodies move (motion detection).
    -   Kinetic energy is conserved or behaves expectedly.
    -   State remains finite (no NaNs).

### 2. Visual Verification

Once the backend is verified, you can focus on rendering:

1.  **Run with Rendering**:
    ```bash
    ./atlascore_app [scenario_name]
    ```
    If the simulation behaves correctly in headless mode but looks wrong here, the issue is likely in the `Render` method of the scenario or the `TextRenderer` itself.

2.  **Text Renderer Tests**:
    `atlascore_text_renderer_tests` verifies the drawing primitives of the ASCII renderer.

## Debugging Steps

If you encounter a bug:

1.  **Isolate**: Run `ctest`. If a specific test fails, debug that test.
2.  **Headless Check**: If tests pass but the app crashes, run `./atlascore_app --headless`.
    -   If it crashes, the bug is in `Update` or core systems.
    -   If it runs, the bug is likely in `Render`.
3.  **Visual Check**: If it runs but looks wrong, check the `Render` implementation.

## Adding New Features

When adding a new feature (e.g., a new physics constraint):

1.  Implement the logic.
2.  Add a unit test in `tests/`.
3.  Create a scenario in `src/simlab/`.
4.  Implement `Update` to use the feature.
5.  Implement `Render` to visualize it.
6.  Verify with `ctest` and headless run before relying on visual output.
