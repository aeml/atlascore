# AtlasCore System Architecture

## 1. Overview

**AtlasCore** is a C++ simulation framework demonstrating:

- A high-performance **multithreaded job system**
- A modular, extensible **Entity–Component–System (ECS)** architecture
- A deterministic **physics simulation layer**
- A flexible **Simulation Lab** capable of:
  - Benchmarks and performance profiling
  - Concurrency stress testing
  - Determinism verification across thread counts
  - Optional ASCII-based visualization
  - Exporting simulation data (CSV) for plotting

AtlasCore is designed as a **portfolio-quality systems project** illustrating professional software architecture, concurrency design, and simulation fundamentals.

---

## 2. Design Principles

AtlasCore adheres to the following principles:

- **Polymorphism over if/switch** for dynamic behavior
- **Small, single-responsibility functions**
- **DRY** (Don’t Repeat Yourself) through reusable modules
- **Separation of concerns** between core, jobs, ECS, physics, and scenarios
- **Deterministic simulation** when configured for fixed-step operation
- **Testability** via internal self-tests and world-state hashing

These principles ensure a clean, maintainable, and scalable codebase.

---

## 3. High-Level Architecture

AtlasCore is composed of the following modules:

1. **Core**
   - Logging, timing, and fixed-timestep utilities
2. **Job System**
   - Worker threads, task scheduling, job queues, synchronization
3. **ECS**
   - Entity IDs, component storage, and polymorphic systems
4. **Physics**
   - Physics components and deterministic update systems
5. **Simulation Lab**
   - Configurable scenarios, benchmarks, determinism tests, visualization
6. **ASCII Renderer (Optional)** (See [docs/ascii.md](docs/ascii.md))
   - Lightweight textual visualization for simulation states
7. **Testing & Determinism Framework**
   - Automated verification of consistency and correctness

Data typically flows:

**Application → Scenario → ECS/Physics → Job System → Core**

All modules are loosely coupled, with the Job System independent from ECS and Physics.

---

## 4. Module Architecture

### 4.1 Core Module

(See [docs/core.md](docs/core.md))

**Namespace:** `core`

**Responsibilities:**
- Logging and diagnostics
- High-precision time utilities
- Fixed-timestep simulation loops

**Key Components:**

#### `Logger`
- Handles engine output
- Methods:
  - `Info(const std::string&)`
  - `Warn(const std::string&)`
  - `Error(const std::string&)`

#### `Clock`
- Provides system timing
- Methods:
  - `NowSeconds()`
  - `NowMicroseconds()`

#### `FixedTimestepLoop`
- Manages deterministic stepping
- Methods:
  - `Run(update, running_flag)`

Each class performs a *single* responsibility.

---

### 4.2 Job System Module

(See [docs/jobs.md](docs/jobs.md))

**Namespace:** `jobs`

**Responsibilities:**
- High-throughput task scheduling
- Worker thread management
- Dependency handling via job handles

**Key Components:**

#### `IJob`
- Base interface for all jobs
- Method:
  - `virtual void Execute() = 0;`

#### `JobHandle`
- Represents a scheduled job
- Allows waiting for completion

#### `JobQueue`
- Thread-safe or lock-minimal queue
- Handles job storage and retrieval

#### `JobSystem`
- Owns the worker threads and job queues
- Methods:
  - `Schedule(std::unique_ptr<IJob>)`
  - `template <typename T, Args...> ScheduleJob(Args&&...)`
  - `Wait(JobHandle&)`

The Job System is intentionally *agnostic* of ECS or physics.

---

### 4.3 ECS Module

(See [docs/ecs.md](docs/ecs.md))

**Namespace:** `ecs`

**Responsibilities:**
- Entity management
- Component storage and access
- Polymorphic update systems

**Key Components:**

#### `EntityId`
- Lightweight integer identifier

#### `IComponent`
- Base interface for all components

#### `ComponentStorage<T>`
- Responsible for:
  - Creating, removing, and accessing components of type `T`
  - Implements a **Sparse Set** (Dense Vector + Sparse Map) for cache-friendly iteration and O(1) access.
  - Exposes raw data vectors for parallel job processing.

#### `ISystem`
- Base interface for all systems
- Method:
  - `Update(World&, float dt)`

#### `World`
- Central ECS manager
- Methods:
  - `CreateEntity()`
  - `DestroyEntity()`
  - `AddComponent<T>(entity, component)`
  - `GetComponent<T>(entity)`
  - `AddSystem(std::unique_ptr<ISystem>)`
  - `Update(dt)`

Systems may internally create jobs to parallelize their work.

---

### 4.4 Physics Module

(See [docs/physics.md](docs/physics.md))

**Namespace:** `physics`

**Responsibilities:**
* Deterministic integration of rigid bodies (fixed timestep)
* Collision detection & impulse-based resolution
* Constraint / joint solving (distance joints)
* Optional parallel execution where safe (integration, collision pass partitioning)

**Implemented Components:**

* `TransformComponent` – Position (x,y) + rotation
* `RigidBodyComponent` – Velocities, mass/inertia, friction, restitution, angular state, PBD backup positions
* `AABBComponent` – Broad‑phase axis-aligned bounds
* `CircleColliderComponent` – Simple circle collider + local offset
* `DistanceJointComponent` – Soft/rigid distance constraint with compliance parameter
* `EnvironmentForces` – Global gravity, wind, drag

Helper inertia configuration functions (`ConfigureCircleInertia`, `ConfigureBoxInertia`) maintain consistent rotational dynamics.

**Systems Pipeline:**

1. `PhysicsIntegrationSystem` – Applies environment forces; integrates velocities & transforms; maintains deterministic ordering; optionally parallelizable
2. `CollisionSystem` – Broad‑phase AABB overlap detection generating `CollisionEvent` list
3. `ConstraintResolutionSystem` – Iterative distance joint enforcement
4. `CollisionResolutionSystem` – Impulse + positional correction (separate velocity/position iterations)
5. `PhysicsSystem` – Orchestrator bundling the above, exposing tuning via `PhysicsSettings` (substeps, iteration counts, slop/correction factors)

Determinism is preserved through fixed iteration orders and controlled parallel partitioning (no unordered reductions). World hashing includes transform state, rigid body state, AABB bounds, and collision event counts.


---

## 5. Simulation Lab

(See [docs/simlab.md](docs/simlab.md))

**Namespace:** `simlab`

Provides interactive or automated scenarios demonstrating AtlasCore’s functionality.

### Scenario Types

#### **Showcase Scenarios**
- **Planetary Gravity**: N-Body simulation demonstrating gravity.
- **Wrecking Ball**: Demonstrates distance joints and collision resolution.
- **Particle Fluid**: High entity count simulation with enclosed container.

#### **Determinism Verification**
- Scenarios use `WorldHasher` to verify state consistency.
- `atlascore_simlab_scenarios_tests` runs scenarios in headless mode to ensure deterministic behavior.

#### **ASCII Visualization**
- Textual “rendering”
- Useful for demonstrating:
  - Falling objects
  - Collisions
  - Movement patterns

#### **CSV Export Mode**
- Dumps per-step data:
  - Positions
  - Velocities
  - Collisions
  - Timing
- For external plotting (e.g., Python/matplotlib)

---

## 6. Testing & Determinism

AtlasCore includes:

### **Automated Self-Tests**
- Entity creation
- Component add/remove
- Physics integration correctness
- Collision resolution sanity checks
- Job completion verification

### **World Hashing**
- Stable hashing of:
  - Positions
  - Velocities
  - Component values
- Used for determinism verification

### Example Output
[PASS] Entity lifecycle
[PASS] Component storage
[PASS] Physics integration
[PASS] Collision resolution
[PASS] Job execution
[PASS] Determinism (hash matched across runs)

---

## 7. Concurrency Model

- Worker threads: `hardware_concurrency - 1` by default  
- Work-stealing or lock-minimal queues  
- Systems may schedule job batches per component or entity “chunk”  
- JobHandles synchronize where necessary  
- Designed to maintain determinism in configured modes

---

## 8. Extensibility

AtlasCore is intentionally modular:

- Add new components → no changes to ECS core
- Add new systems → extend `ISystem`
- Add new job types → extend `IJob`
- Add new simulation scenarios → extend `IScenario`
- Replace physics → plug in new systems
- Integrate allocators → back ECS storages or job queues

The architecture promotes long-term evolution without modifying core layers.

---

## 9. Summary

AtlasCore is a compact but professionally-structured C++ framework demonstrating:

- Strong concurrency fundamentals  
- ECS architectural mastery  
- Clean physics simulation design  
- Deterministic behavior  
- Testability and modularity  
- Real-world engine practices

