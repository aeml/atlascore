#include "core/Clock.hpp"
#include "core/FixedTimestepLoop.hpp"
#include "core/Logger.hpp"
#include "jobs/JobSystem.hpp"
#include "physics/Systems.hpp"
#include "physics/Components.hpp"
#include "ecs/World.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    core::Logger logger;
    logger.Info("Running AtlasCore self-tests...");

    // Basic Clock sanity check
    const auto t1 = core::Clock::NowMicroseconds();
    const auto t2 = core::Clock::NowMicroseconds();
    assert(t2 >= t1);

    // Basic FixedTimestepLoop check: ensure it calls update
    std::atomic<bool> running{true};
    core::FixedTimestepLoop loop{1.0f / 60.0f};

    int updateCount = 0;

    // Run loop briefly in the same thread, stop after a few updates
    loop.Run(
        [&](float dt)
        {
            (void)dt;
            ++updateCount;
            if (updateCount >= 3)
            {
                running.store(false);
            }
        },
        running);

    assert(updateCount >= 3);

    // Jobs: schedule a simple function and ensure workers exist
    jobs::JobSystem jobSystem;
    assert(jobSystem.WorkerCount() > 0);

    std::atomic<int> jobCounter{0};
    jobSystem.ScheduleFunction([
        &jobCounter
    ]
    {
        ++jobCounter;
    });

    // Sleep briefly to allow the job to run
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(jobCounter.load() >= 1);

    // Physics integration: single body should move under gravity.
    physics::PhysicsIntegrationSystem physicsSystem;
    std::vector<physics::TransformComponent> transforms(1);
    std::vector<physics::RigidBodyComponent> bodies(1);
    transforms[0].y = 10.0f;
    bodies[0].vy = 0.0f;
    const float dt = 1.0f / 60.0f;
    physicsSystem.Integrate(transforms, bodies, dt);
    // After one step, vy should be gravity * dt (negative) and y decreased.
    assert(bodies[0].vy < 0.0f);
    assert(transforms[0].y < 10.0f);

    // Parallel Physics integration test
    {
        physics::PhysicsIntegrationSystem parallelPhysics;
        parallelPhysics.SetJobSystem(&jobSystem);

        const size_t count = 300;
        std::vector<physics::TransformComponent> pTransforms(count);
        std::vector<physics::RigidBodyComponent> pBodies(count);

        for (size_t i = 0; i < count; ++i)
        {
            pTransforms[i].y = 10.0f;
            pBodies[i].vy = 0.0f;
        }

        parallelPhysics.Integrate(pTransforms, pBodies, dt);

        for (size_t i = 0; i < count; ++i)
        {
            assert(pBodies[i].vy < 0.0f);
            assert(pTransforms[i].y < 10.0f);
        }
    }

    // ECS component storage integration: add and retrieve a TransformComponent.
    ecs::World ecsWorld;
    auto e = ecsWorld.CreateEntity();
    auto& ecsTransform = ecsWorld.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{1.0f, 2.0f});
    assert(ecsTransform.x == 1.0f && ecsTransform.y == 2.0f);
    auto* ecsFetched = ecsWorld.GetComponent<physics::TransformComponent>(e);
    assert(ecsFetched != nullptr);
    assert(ecsFetched->x == 1.0f && ecsFetched->y == 2.0f);

    logger.Info("[PASS] Core and Jobs self-tests");
    std::cout << "All self-tests passed.\n";
    return 0;
}
