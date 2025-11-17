# AtlasCore

AtlasCore is a C++ simulation framework showcasing a multithreaded job system, an Entity–Component–System (ECS) architecture, deterministic physics, and a small Simulation Lab for benchmarks and determinism tests.

This repository currently contains a **minimal evolving skeleton**:

- `core` module with `Logger`, `Clock`, and `FixedTimestepLoop`
- `jobs` module with a simple `JobSystem` interface and worker pool
- `ecs` module with a basic `World` / `ISystem` abstraction (component storage integration upcoming)
- `physics` module with `TransformComponent`, `RigidBodyComponent`, and gravity integration via `PhysicsIntegrationSystem`
- `ascii` module providing a simple textual renderer of entity positions
- `simlab` module featuring a determinism smoke-test scenario now demonstrating a falling-body demo
 - `simlab` module featuring a falling-body demo and a determinism hash scenario (dual-run hash comparison)
- A small `main` entry point that runs a short 60-step simulation
- A basic self-test executable (CTest) covering core, jobs, and physics integration

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
# Run the demo app
./atlascore_app

# Run self-tests (if CTest is available)
ctest -C Release
```

## Next Steps

- Wire full ECS component storage into the `World`
- Add determinism hashing harness
 - Expand determinism tests (collision, multi-thread job scheduling impact)
- Expand ASCII visualization / add optional CSV export utilities
- Implement collision detection & resolution systems
- Introduce benchmarking scenarios for job throughput and determinism
