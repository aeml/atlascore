# Coverage

AtlasCore uses GCC/Clang `--coverage` instrumentation (gcov) enabled via the CMake option:

```
-DATLASCORE_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
```

## Generating Coverage Locally (Linux)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DATLASCORE_ENABLE_COVERAGE=ON
cmake --build build --config Debug --parallel
cd build
ctest -C Debug --output-on-failure
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

Open `coverage-report/index.html` in a browser.

## CI & Publishing

On pushes to `main`, GitHub Actions generates coverage on Linux and publishes:

- Doxygen API documentation at the GitHub Pages root.
- The HTML coverage report under `/coverage`.

## Future Improvements

- Add branch coverage flags (e.g. `-fprofile-arcs -ftest-coverage`) explicitly if needed.
- Integrate LLVM's `llvm-cov` for Clang-specific richer reports.
- Tune the CI threshold as coverage evolves (currently enforced in CI at 65% lines).
