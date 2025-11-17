# AtlasCore ![CI](https://github.com/aeml/atlascore/actions/workflows/ci.yml/badge.svg) [![Coverage Report](https://img.shields.io/badge/coverage-%2Fcoverage-blue)](https://aeml.github.io/atlascore/coverage/) [![Docs](https://img.shields.io/badge/docs-doxygen-green)](https://aeml.github.io/atlascore/)

AtlasCore is a C++ simulation framework showcasing a multithreaded job system, an Entity–Component–System (ECS) architecture, deterministic physics, and a small Simulation Lab for interactive demos and determinism tests, rendered entirely as text.

This repository currently contains a **minimal but growing engine**:

- `core` module with `Logger`, `Clock`, and `FixedTimestepLoop` for driving fixed-timestep simulations
- `jobs` module with a simple `JobSystem` interface and worker pool for parallel workloads
- `ecs` module with a `World` / `ISystem` abstraction and integrated component storage (type-erased wrappers over `ComponentStorage<T>`) 
- `physics` module with `TransformComponent`, `RigidBodyComponent`, gravity integration via `PhysicsIntegrationSystem`, and basic collision helpers
- `ascii` module providing an ANSI-friendly `TextRenderer` for efficient terminal rendering (diff-based text frames)
- `simlab` module featuring:
	- `smoke` demo: ECS bodies falling under physics, visualized as continuously dropping characters
	- `text_patterns` demo: animated sine waves and Lissajous patterns using the `TextRenderer`
	- determinism/hash scenarios for dual-run comparison
- A `main` entry point that lets you pick and run scenarios interactively
- A CTest suite covering core, jobs, physics, ECS, and simlab behavior

Refer to `sysarchitecture.md` for the high-level design and roadmap.

## Building (Windows / CMake + MSVC)

Prerequisites:

- CMake 3.20+
- Visual Studio with C++ toolset (or Build Tools)

From the project root (`atlascore`):

```powershell
mkdir build
cd build
cmake -G "Ninja" ..    # or "Visual Studio 17 2022" if you prefer
cmake --build . --config Release
```

## Running

From the `build` directory:

```powershell
# Run the demo app (interactive scenario menu)
./atlascore_app

# Or pick a scenario via CLI argument
./atlascore_app smoke
./atlascore_app hash

# Run self-tests (if CTest is available)
ctest -C Release
```

## Next Steps / Roadmap

Short-term goals:

- Make the Simulation Lab a showcase of the engine:
	- Refine the **smoke** demo (soft drifting + continuous spawn)
	- Add a **rain** demo (fast vertical motion, optional bouncing at ground)
	- Add a **clouds** demo (slow horizontal motion, layering, wind-style effects)
	- Integrate the **job system** to drive large numbers of entities in these demos
- Expand ASCII visualization patterns and utilities (e.g., debug overlays, simple HUD text)
- Expand determinism tests (collision, multi-thread job scheduling impact)

Engine-oriented work:

- Optimize ECS storage layout (chunked / archetype approach)
- Improve physics (collision detection, resolution, constraints)
- Add simple profiling/metrics hooks for job throughput and ECS/physics performance
- Introduce dedicated benchmarking scenarios for job throughput, ECS iteration, and determinism

## Code Coverage

Coverage is generated on Linux builds when `ATLASCORE_ENABLE_COVERAGE=ON`.

Local run:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DATLASCORE_ENABLE_COVERAGE=ON
cmake --build build --config Debug --parallel
cd build && ctest -C Debug --output-on-failure
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

After each push to `main`:

- Doxygen API documentation is published at the GitHub Pages root.
- Coverage HTML report is published under `/coverage`.
