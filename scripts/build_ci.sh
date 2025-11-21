#!/bin/bash
set -e

# Define build directory (using build-linux to avoid Windows conflicts in WSL)
BUILD_DIR="build-linux"

echo "Configuring..."
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Debug -DATLASCORE_ENABLE_COVERAGE=ON -DATLASCORE_BUILD_TESTS=ON

echo "Building..."
cmake --build $BUILD_DIR --config Debug --parallel

echo "Running Tests..."
cd $BUILD_DIR
ctest -C Debug --output-on-failure

echo "Collecting Coverage..."
lcov --directory . --capture --output-file coverage.base.info
# Remove system and test files
lcov --remove coverage.base.info '/usr/*' '*/tests/*' --output-file coverage.info
lcov --list coverage.info

echo "Done."
