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

## Codecov

Configure the GitHub secret `CODECOV_TOKEN` for private repos. The CI workflow (`ci.yml`) uploads `coverage.info` and uses Codecov action if the token is present.

## Future Improvements

- Add branch coverage flags (e.g. `-fprofile-arcs -ftest-coverage`) explicitly if needed.
- Integrate LLVM's `llvm-cov` for Clang-specific richer reports.
- Add threshold enforcement (fail CI if coverage < target).