# Jobs Module

The `jobs` module implements a minimal worker-pool based job system.

- `IJob`: interface with `Execute()`.
- `JobHandle`: opaque handle for scheduled jobs.
- `JobSystem`: owns worker threads, supports `Schedule`, `ScheduleFunction`, and `Dispatch`.

`Wait` blocks the calling thread until the specified job (or list of jobs) is complete.

## Parallel Dispatch

The `Dispatch` method allows splitting a loop over a range `[0, count)` into parallel batches.

```cpp
jobSystem.Dispatch(count, batchSize, [&](size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
        // work
    }
});
```
