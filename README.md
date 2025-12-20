# AtlasCore ![CI](https://github.com/aeml/atlascore/actions/workflows/ci.yml/badge.svg) [![Coverage Report](https://img.shields.io/badge/coverage-%2Fcoverage-blue)](https://aeml.github.io/atlascore/coverage/) [![Docs](https://img.shields.io/badge/docs-doxygen-green)](https://aeml.github.io/atlascore/)

AtlasCore is a modern C++20 simulation framework demonstrating:

* Multithreaded job scheduling (work queues + optional parallel physics/collision)
* An Entity–Component–System (ECS) with type‑erased component registries
* Deterministic, fixed‑timestep physics (integration, collision detection, resolution, constraints)
* ASCII (ANSI) text rendering for lightweight visualization (diff/frame based)
* A Simulation Lab of interactive + headless scenarios (benchmarks, determinism tests, visual demos)
* Extensive self‑tests + determinism hashing across scenarios

See [sysarchitecture.md](sysarchitecture.md) for deeper design notes. Module‑specific docs live under [docs/](docs/).

## Modules

| Module   | Key Types / Responsibilities |
|----------|------------------------------|
| core     | `Logger`, `Clock`, `FixedTimestepLoop` (stable fixed delta) |
| jobs     | `JobSystem` (worker pool, scheduling, optional parallelization hooks) |
| ecs      | `World`, `ComponentStorage<T>`, `ISystem` polymorphic updates |
| physics  | Components: `TransformComponent`, `RigidBodyComponent`, `DistanceJointComponent`, `EnvironmentForces`, `AABBComponent`, `CircleColliderComponent`; Systems: `PhysicsIntegrationSystem`, `CollisionSystem`, `CollisionResolutionSystem`, `ConstraintResolutionSystem`, `PhysicsSystem` orchestrator |
| ascii    | `TextRenderer` + `Renderer` (terminal diff rendering) |
| simlab   | Scenario registry, hashing tools, built‑in scenarios (see below) |

## Scenarios (SimLab)

Scenario keys (CLI):

```
gravity          # Planetary Gravity (N-Body)
wrecking         # Wrecking Ball (Joints & Collisions)
fluid            # Particle Fluid (High Entity Count)
```

Run interactively (menu-driven) with `./atlascore_app` (no args) or pick a key: `./atlascore_app fluid`. Use `--headless` or env `ATLASCORE_HEADLESS=1` to stream output to `headless_output.txt`.

## Directory Structure (Public Headers)

```
include/
  core/      Clock.hpp, FixedTimestepLoop.hpp, Logger.hpp
  jobs/      JobSystem.hpp
  ecs/       World.hpp, ComponentStorage.hpp
  physics/   Components.hpp, Systems.hpp, CollisionSystem.hpp
  ascii/     TextRenderer.hpp, Renderer.hpp
  simlab/    Scenario.hpp (plus registry + hashing in src/simlab)
src/         (module implementations)
tests/       (CTest executables driven by CMake options)
docs/        (module docs + workflows)
```

## Building

Prerequisites: CMake ≥ 3.20, a C++20 compiler (MSVC 17+, Clang, or GCC). Optional: Ninja.

Windows (MSVC):
```powershell
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Debug --parallel
```

Linux / macOS (Clang/GCC):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Options:
* `ATLASCORE_BUILD_TESTS=ON|OFF` (default ON)
* `ATLASCORE_ENABLE_COVERAGE=ON` (GNU/Clang only; adds `--coverage -O0 -g -fprofile-update=atomic`)

## Running & CLI

```bash
./atlascore_app                 # interactive menu
./atlascore_app cloth           # specific scenario
./atlascore_app stress --headless
ATLASCORE_HEADLESS=1 ./atlascore_app smoke  # env flag alternative
```

Headless output file: `headless_output.txt` (overwritten per run).

## Testing

CTest targets (enabled when `ATLASCORE_BUILD_TESTS=ON`) include:

`atlascore_selftests`, `atlascore_determinism_tests`, `atlascore_collision_tests`, `atlascore_determinism_collision_tests`, `atlascore_text_renderer_tests`, `atlascore_text_renderer_extra_tests`, `atlascore_ecs_extra_tests`, `atlascore_ecs_physics_tests`, `atlascore_physics_stability_tests`, `atlascore_simlab_scenarios_tests`, `atlascore_ecs_collision_tests`, `atlascore_jobs_wait_tests`, `atlascore_scenario_registry_tests`, `atlascore_coverage_tests`.

Run all:
```bash
ctest --output-on-failure
```
Specific target:
```bash
ctest -R AtlasCoreCollisionTests
```

## Determinism

Determinism tests hash world state (transform positions, velocities, collision counts, joint and rigid body data). The `hash` scenario performs dual runs to confirm invariants across thread counts. Use fixed timestep loop (`1/60s`) to maintain sim stability.

## Coverage (GNU/Clang)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DATLASCORE_ENABLE_COVERAGE=ON
cmake --build build --parallel
cd build && ctest --output-on-failure
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

GitHub Actions publishes Doxygen docs and coverage HTML (`/coverage`).

## Roadmap (High-Level)

Short term:
* Expand scenario variety (rain, clouds, profiling‑oriented benchmarks)
* Additional physics constraints & joint compliance behaviors
* ECS storage optimization (move toward contiguous / archetype iteration)
* Lightweight profiling overlays through ASCII renderer (HUD + metrics)

Longer term:
* More robust collision shapes (rotated boxes, line segments)
* Parallel solver phases with determinism controls
* Scenario scripting / data export formats beyond headless text

## Contributing

See [docs/contributing.md](docs/contributing.md) and module docs (`docs/*.md`). Please keep changes small, add/extend tests, and preserve deterministic behavior when modifying physics or scheduling.

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for details.

## Author

Created by [Robert Mendola](https://mendola.tech)

---
Enjoy exploring AtlasCore – feedback & improvements welcome.
