cmake_minimum_required(VERSION 3.20)
project(AtlasCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ATLASCORE_BUILD_TESTS "Build AtlasCore tests" ON)
option(ATLASCORE_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

add_library(atlascore
    src/core/Logger.cpp
    src/core/Clock.cpp
    src/core/FixedTimestepLoop.cpp
    src/jobs/JobSystem.cpp
    src/ecs/World.cpp
    src/physics/Systems.cpp
    src/ascii/Renderer.cpp
    src/ascii/TextRenderer.cpp
    src/simlab/DeterminismSmokeTest.cpp
    src/simlab/WorldHasher.cpp
    src/simlab/DeterminismHashScenario.cpp
)

# Public headers
target_include_directories(atlascore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Warnings
if (MSVC)
    target_compile_options(atlascore PRIVATE /W4 /permissive-)
else()
    target_compile_options(atlascore PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(atlascore_app src/main.cpp)

target_link_libraries(atlascore_app PRIVATE atlascore)

# Coverage instrumentation (GNU/Clang). Applied only if explicitly enabled.
if (ATLASCORE_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Coverage enabled")
    target_compile_options(atlascore PRIVATE --coverage -O0 -g)
    target_link_options(atlascore PRIVATE --coverage)
    if (ATLASCORE_BUILD_TESTS)
        # Apply to test targets after they are created (below we handle selftests + determinism)
    endif()
endif()

if (ATLASCORE_BUILD_TESTS)
    enable_testing()
    add_executable(atlascore_selftests tests/selftests.cpp)
    target_link_libraries(atlascore_selftests PRIVATE atlascore)
    add_test(NAME AtlasCoreSelfTests COMMAND atlascore_selftests)

    add_executable(atlascore_determinism_tests tests/determinism_tests.cpp)
    target_link_libraries(atlascore_determinism_tests PRIVATE atlascore)
    add_test(NAME AtlasCoreDeterminismTests COMMAND atlascore_determinism_tests)

    add_executable(atlascore_text_renderer_tests tests/text_renderer_tests.cpp)
    target_link_libraries(atlascore_text_renderer_tests PRIVATE atlascore)
    add_test(NAME AtlasCoreTextRendererTests COMMAND atlascore_text_renderer_tests)

    if (ATLASCORE_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(atlascore_selftests PRIVATE --coverage -O0 -g)
        target_link_options(atlascore_selftests PRIVATE --coverage)
        target_compile_options(atlascore_determinism_tests PRIVATE --coverage -O0 -g)
        target_link_options(atlascore_determinism_tests PRIVATE --coverage)
        target_compile_options(atlascore_text_renderer_tests PRIVATE --coverage -O0 -g)
        target_link_options(atlascore_text_renderer_tests PRIVATE --coverage)
    endif()
endif()
