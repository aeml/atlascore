# Jobs Module

The `jobs` module implements a minimal worker-pool based job system.

- `IJob`: interface with `Execute()`.
- `JobHandle`: opaque handle for scheduled jobs.
- `JobSystem`: owns worker threads, supports `Schedule` and `ScheduleFunction`.

`Wait` is currently a stub and will later provide per-job completion tracking.
