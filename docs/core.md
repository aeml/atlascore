# Core Module

The `core` module provides logging, timing, and fixed-timestep loop utilities.

- `Logger`: thread-safe, timestamped logging with `Info`, `Warn`, `Error`.
- `Clock`: monotonic `NowSeconds` and `NowMicroseconds` helpers.
- `FixedTimestepLoop`: runs a fixed-step update function using a shared running flag.
