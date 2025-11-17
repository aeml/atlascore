# AtlasCore

AtlasCore is a C++ simulation framework showcasing a multithreaded job system, an Entity–Component–System (ECS) architecture, deterministic physics, and a small Simulation Lab for benchmarks and determinism tests.

This repository currently contains a **minimal skeleton**:

- `core` module with `Logger`, `Clock`, and `FixedTimestepLoop`
- `jobs` module with a simple `JobSystem` interface and worker pool
- `ecs` module with a basic `World` and `ISystem` abstraction
- `simlab` module with a stub determinism smoke-test scenario
- A small `main` entry point that wires these together
- A basic self-test executable driven by CTest

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

- Flesh out `jobs`, `ecs`, `physics`, and `simlab` modules
- Add ASCII visualization and CSV export utilities
- Implement determinism tests and world hashing as described in `sysarchitecture.md`
