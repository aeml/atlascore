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
    src/physics/CollisionSystem.cpp
    
    src/ascii/Renderer.cpp
    src/ascii/TextRenderer.cpp
    src/simlab/WorldHasher.cpp
    src/simlab/ScenarioRegistry.cpp
    src/simlab/PlanetaryGravityScenario.cpp
    src/simlab/WreckingBallScenario.cpp
    src/simlab/ParticleFluidScenario.cpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(atlascore PRIVATE ATLASCORE_DEBUG_SIMLAB=1)
endif()

# Public headers
target_include_directories(atlascore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Warnings and Compiler Options
if (MSVC)
    target_compile_options(atlascore PUBLIC /W4 /permissive- /EHsc)
else()
    target_compile_options(atlascore PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(atlascore_app src/main.cpp)

target_link_libraries(atlascore_app PRIVATE atlascore)

# Apply coverage flags to app executable as well (needed for linking gcov symbols)
if (ATLASCORE_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(atlascore_app PRIVATE --coverage -O0 -g -fprofile-update=atomic)
    target_link_options(atlascore_app PRIVATE --coverage)
endif()

# Coverage instrumentation (GNU/Clang). Applied only if explicitly enabled.
if (ATLASCORE_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Coverage enabled")
    # -fprofile-update=atomic is needed to prevent data races in coverage counters
    # which can lead to negative execution counts and geninfo errors.
    target_compile_options(atlascore PRIVATE --coverage -O0 -g -fprofile-update=atomic)
    target_link_options(atlascore PRIVATE --coverage)
    if (ATLASCORE_BUILD_TESTS)
        # Apply to test targets after they are created (below we handle selftests + determinism)
    endif()
endif()

if (ATLASCORE_BUILD_TESTS)
    enable_testing()

    function(atlascore_add_test_executable target source test_name)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE atlascore)
        add_test(NAME ${test_name} COMMAND ${target})

        if (ATLASCORE_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE --coverage -O0 -g -fprofile-update=atomic)
            target_link_options(${target} PRIVATE --coverage)
        endif()
    endfunction()

    atlascore_add_test_executable(atlascore_selftests tests/selftests.cpp AtlasCoreSelfTests)
    atlascore_add_test_executable(atlascore_determinism_tests tests/determinism_tests.cpp AtlasCoreDeterminismTests)
    atlascore_add_test_executable(atlascore_text_renderer_tests tests/text_renderer_tests.cpp AtlasCoreTextRendererTests)
    atlascore_add_test_executable(atlascore_collision_tests tests/collision_tests.cpp AtlasCoreCollisionTests)
    atlascore_add_test_executable(atlascore_determinism_collision_tests tests/determinism_collision_tests.cpp AtlasCoreDeterminismCollisionTests)
    atlascore_add_test_executable(atlascore_text_renderer_extra_tests tests/text_renderer_extra_tests.cpp AtlasCoreTextRendererExtraTests)
    atlascore_add_test_executable(atlascore_ecs_extra_tests tests/ecs_extra_tests.cpp AtlasCoreEcsExtraTests)
    atlascore_add_test_executable(atlascore_ecs_physics_tests tests/ecs_physics_tests.cpp AtlasCoreEcsPhysicsTests)
    atlascore_add_test_executable(atlascore_physics_stability_tests tests/physics_stability_tests.cpp AtlasCorePhysicsStabilityTests)
    atlascore_add_test_executable(atlascore_physics_circle_broadphase_tests tests/physics_circle_broadphase_tests.cpp AtlasCorePhysicsCircleBroadphaseTests)
    atlascore_add_test_executable(atlascore_simlab_scenarios_tests tests/simlab_scenarios_tests.cpp AtlasCoreSimlabScenariosTests)
    atlascore_add_test_executable(atlascore_scenario_update_contract_tests tests/scenario_update_contract_tests.cpp AtlasCoreScenarioUpdateContractTests)
    atlascore_add_test_executable(atlascore_ecs_collision_tests tests/ecs_collision_tests.cpp AtlasCoreEcsCollisionTests)
    atlascore_add_test_executable(atlascore_jobs_wait_tests tests/jobs_wait_tests.cpp AtlasCoreJobsWaitTests)
    atlascore_add_test_executable(atlascore_scenario_registry_tests tests/scenario_registry_tests.cpp AtlasCoreScenarioRegistryTests)
    atlascore_add_test_executable(atlascore_coverage_tests tests/coverage_tests.cpp AtlasCoreCoverageTests)
endif()
