# AtlasCore Implementation Backlog

_Last updated: 2026-02-23_

## Status Legend
- `TODO` = not started
- `IN_PROGRESS` = actively being worked
- `BLOCKED` = waiting on dependency/decision
- `DONE` = completion requirements met

## Global Definition of Done (applies to every item)
An item is only `DONE` when all are true:
1. Code changes merged and build passes locally.
2. Relevant tests added/updated and passing.
3. `ctest --output-on-failure` passes for full suite.
4. Determinism-sensitive changes include deterministic validation.
5. Documentation updated (`README.md`, `docs/*.md`, or `sysarchitecture.md` as needed).
6. No new warnings introduced in CI jobs for supported platforms.

---

## Phase 1 - Correctness First (Highest Priority)

### AC-001 - Single-Step Simulation Contract
- **Status:** TODO
- **Priority:** P0
- **Problem:** Simulation currently steps world twice per frame (scenario + main loop).
- **Scope:**
  - Define and enforce one owner of `world.Update(dt)`:
    - Option A: engine owns stepping, scenarios do scenario-specific logic only.
    - Option B: scenarios own stepping, engine does not step.
  - Apply contract across `src/main.cpp` and all scenarios.
- **Completion Requirements:**
  - Exactly one `world.Update(dt)` is executed per frame.
  - `IScenario` behavior is documented and consistent with implementation.
  - Existing scenario tests are updated to match the contract.
- **Verification:**
  - Add/adjust test that fails if a frame executes two world updates.
  - Run full `ctest --output-on-failure`.

### AC-002 - Zero/Invalid `dt` Safety in Physics
- **Status:** TODO
- **Priority:** P0
- **Problem:** Velocity reconstruction can divide by zero when `dt <= 0`.
- **Scope:**
  - Guard physics update paths against `dt <= 0`.
  - Ensure no NaN/Inf propagation from invalid timesteps.
- **Completion Requirements:**
  - No divide-by-zero in physics pipeline for zero timestep.
  - Behavior for `dt <= 0` is defined and documented (no-op or epsilon clamp).
- **Verification:**
  - Add tests for `world.Update(0.0f)` with dynamic rigid bodies.
  - Assert finite transform/velocity outputs.

### AC-003 - JobSystem Exception Safety
- **Status:** TODO
- **Priority:** P0
- **Problem:** Throwing jobs can skip completion signaling and deadlock waiters.
- **Scope:**
  - Wrap job execution with exception capture.
  - Always mark job as completed and notify waiters.
  - Optionally propagate exception on `Wait`.
- **Completion Requirements:**
  - `Wait` never hangs when a job throws.
  - Exception behavior is deterministic and documented.
- **Verification:**
  - Add tests:
    - job throws -> `Wait` returns.
    - multiple jobs with one throwing -> all waits complete.
  - Run stress test with repeated throwing jobs.

---

## Phase 2 - Collision Correctness and Physics Integrity

### AC-004 - Collider Broadphase Unification
- **Status:** TODO
- **Priority:** P0
- **Problem:** Broadphase currently processes only AABBs; circle-only entities may miss collisions.
- **Scope:**
  - Ensure all colliders participate in broadphase (AABB + circle at minimum).
  - Introduce collider broadphase proxy generation per step.
- **Completion Requirements:**
  - Circle-circle and circle-AABB candidates are generated in broadphase.
  - No scenario relies on manually adding AABB just to be collidable.
- **Verification:**
  - Add tests:
    - circle-only dynamic pair collides and resolves.
    - circle-vs-static-wall collision detected without manual per-entity hacks.
  - Deterministic results across repeated runs.

### AC-005 - World-Space AABB Source of Truth
- **Status:** TODO
- **Priority:** P1
- **Problem:** Some scenarios initialize AABBs in local space while systems treat them as world-space.
- **Scope:**
  - Standardize AABB representation.
  - Recompute world-space broadphase bounds from colliders + transforms each substep.
- **Completion Requirements:**
  - Dynamic AABBs are always consistent with current transform.
  - Scenario setup no longer uses ambiguous local/world AABB values.
- **Verification:**
  - Add tests asserting AABB tracks moving body correctly over multiple frames.
  - Validate wrecking/particle scenarios maintain expected collisions.

### AC-006 - Scenario Physics Data Cleanup
- **Status:** TODO
- **Priority:** P1
- **Problem:** Scenario setup currently mixes collider assumptions and broadphase requirements.
- **Scope:**
  - Update scenario entity setup to match unified collider model.
  - Remove workaround-style setup comments/logic.
- **Completion Requirements:**
  - `gravity`, `wrecking`, and `fluid` scenarios run with stable collisions under new model.
  - Headless and rendered behavior remain finite and deterministic.
- **Verification:**
  - `tests/simlab_scenarios_tests.cpp` passes with tightened assertions.
  - Add scenario-specific collision sanity checks.

---

## Phase 3 - ECS Lifecycle Correctness

### AC-007 - Component Removal on Entity Destroy
- **Status:** TODO
- **Priority:** P1
- **Problem:** Destroyed entities can leave orphaned components in storages.
- **Scope:**
  - Add `Remove(EntityId)` to component storage.
  - Extend type-erased storage interface to remove entity entries.
  - Call removal for all component stores in `World::DestroyEntity`.
- **Completion Requirements:**
  - Destroyed entity has no retrievable components in any store.
  - Iteration APIs do not surface destroyed entities.
- **Verification:**
  - Add tests:
    - create -> add components -> destroy -> all `GetComponent` return null.
    - component count decreases as expected.
  - Full suite passes unchanged behavior elsewhere.

### AC-008 - ECS Type-Key Hardening
- **Status:** TODO
- **Priority:** P2
- **Problem:** `typeid(T).hash_code()` keying is less robust than `std::type_index`.
- **Scope:**
  - Replace hash-code map keys with `std::type_index` where practical.
- **Completion Requirements:**
  - No behavior regressions in component add/get/for-each/view.
  - Internal casts and storage boundaries remain type-safe.
- **Verification:**
  - ECS unit tests expanded for multiple component types and retrieval stability.

---

## Phase 4 - Runtime and Loop Stability

### AC-009 - Main Loop Thread Lifecycle Hardening
- **Status:** TODO
- **Priority:** P1
- **Problem:** Detached quit thread can outlive context and introduces shutdown nondeterminism.
- **Scope:**
  - Replace detached thread with joinable lifecycle (`std::jthread` preferred where available).
  - Ensure clean shutdown path in interactive and headless mode.
- **Completion Requirements:**
  - No detached thread remains in main runtime path.
  - Shutdown is deterministic and race-free.
- **Verification:**
  - Add test/diagnostic run for fast start/stop cycles.
  - Run with sanitizers (once added) to confirm no lifetime issues.

### AC-010 - FixedTimestepLoop Pacing and Catch-Up Limits
- **Status:** TODO
- **Priority:** P2
- **Problem:** Busy-spin and unlimited catch-up can cause CPU burn/spiral behavior.
- **Scope:**
  - Add idle wait/yield strategy when below timestep.
  - Clamp maximum frame-time catch-up and max sub-iterations.
- **Completion Requirements:**
  - Loop behavior documented for overload conditions.
  - CPU usage in idle/no-op loop is materially reduced.
- **Verification:**
  - Add loop-focused test(s) for bounded update counts under artificial lag.
  - Validate no determinism regression in normal operation.

---

## Phase 5 - Maintainability, CI, and Docs

### AC-011 - Split Physics Monolith (`Systems.cpp`)
- **Status:** TODO
- **Priority:** P2
- **Problem:** Single large file is difficult to reason about and review.
- **Scope:**
  - Split into focused translation units (integration, collision resolution, constraints, orchestration).
- **Completion Requirements:**
  - Build graph unchanged externally; public headers remain coherent.
  - No functional regression in physics tests.
- **Verification:**
  - Full test suite pass; determinism/collision tests unchanged or improved.

### AC-012 - CMake Test/Coverage DRY Refactor
- **Status:** TODO
- **Priority:** P2
- **Problem:** Repetitive target definitions and repeated coverage flags.
- **Scope:**
  - Introduce helper function(s) for registering test executables and coverage instrumentation.
- **Completion Requirements:**
  - CMake file is shorter and easier to maintain.
  - Same test targets and behavior preserved.
- **Verification:**
  - Configure/build/test works on Linux/macOS/Windows CI matrix.

### AC-013 - Add Sanitizer CI Lane
- **Status:** TODO
- **Priority:** P1
- **Problem:** Current CI lacks ASan/UBSan signal for runtime defects.
- **Scope:**
  - Add Linux sanitizer job (Debug + `-fsanitize=address,undefined`).
- **Completion Requirements:**
  - Sanitizer job runs in CI and is required (or clearly non-blocking by policy).
  - No sanitizer errors in baseline branch.
- **Verification:**
  - CI green with sanitizer lane enabled.

### AC-014 - Documentation Drift Cleanup
- **Status:** TODO
- **Priority:** P1
- **Problem:** Several docs and README claims do not match current behavior.
- **Scope:**
  - Fix CLI examples, scenario keys, scenario interface docs, headless behavior notes, and coverage docs.
- **Completion Requirements:**
  - `README.md`, `docs/simlab.md`, `docs/debugging_workflow.md`, `docs/coverage.md`, and `sysarchitecture.md` are consistent with implementation.
- **Verification:**
  - Manual doc consistency pass and reviewer sign-off.

### AC-015 - Repository Hygiene (`.gitignore`)
- **Status:** TODO
- **Priority:** P2
- **Problem:** Unexpected ignore entries (e.g., `README.md`) can hide important changes.
- **Scope:**
  - Remove/justify suspicious ignores.
- **Completion Requirements:**
  - Tracked docs behave normally in git status.
  - Ignore file contains only intentional generated/local artifacts.
- **Verification:**
  - `git status` reflects doc edits correctly.

---

## Suggested Execution Order
1. AC-001, AC-002, AC-003
2. AC-004, AC-005, AC-006
3. AC-007, AC-008
4. AC-009, AC-010
5. AC-013, AC-011, AC-012, AC-014, AC-015

---

## Milestone Exit Criteria

### Milestone A (Core Correctness)
- AC-001/002/003 all `DONE`.
- No hangs, no zero-dt failures, no duplicate step path.
- Full test suite green.

### Milestone B (Collision Reliability)
- AC-004/005/006 all `DONE`.
- Circle and AABB collision paths verified by tests.
- Determinism checks still passing.

### Milestone C (Lifecycle + Stability)
- AC-007/009/010 all `DONE`.
- Entity destroy fully cleans components.
- Main loop shutdown stable; loop pacing improved.

### Milestone D (Engineering Quality)
- AC-011/012/013/014/015 all `DONE`.
- CI includes sanitizer lane.
- Docs and repo hygiene aligned with implementation.
