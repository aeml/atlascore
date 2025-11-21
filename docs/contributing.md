# Contributing & Style Guide

This project is intended as a clean, portfolio-quality C++ systems codebase.

## Coding Style

- Use modern C++ (C++20) features where they improve clarity and safety.
- Prefer small, focused classes and functions with clear responsibilities.
- Avoid large macros and preprocessor-heavy designs.
- Prefer value semantics and RAII; minimize raw `new`/`delete`.
- Use `std::unique_ptr` / `std::shared_ptr` explicitly when ownership semantics matter.
- Prefer `enum class` over plain enums.
- Avoid global state; prefer explicit dependencies.
- Keep headers minimal and stable; include only what you use.

## Naming

- Types: `PascalCase` (e.g., `JobSystem`, `FixedTimestepLoop`).
- Functions and methods: `camelCase` or descriptive verbs (e.g., `CreateEntity`, `Run`).
- Member variables: `m_` prefix (e.g., `m_workers`).
- Namespaces: lower-case (e.g., `core`, `jobs`, `ecs`).

## Modules

See the module docs under `docs/` and [sysarchitecture.md](../sysarchitecture.md) for intent:

- `core`: logging, timing, fixed timestep loop.
- `jobs`: job system and worker pool.
- `ecs`: entities, components, systems.
- `physics`: physics components and systems.
- `simlab`: scenarios, benchmarks, determinism tests.
- `ascii`: text-based visualization.

## Tests

- Prefer small, fast tests under `tests/`.
- Use asserts and simple executables wired into CTest.
- Keep tests deterministic: avoid unbounded loops and timeouts.

## Documentation

- Update `README.md`, [sysarchitecture.md](../sysarchitecture.md), and the relevant `docs/*.md` when you change public behavior.
- Keep comments high-signal; avoid restating obvious code.
