#include "core/Clock.hpp"
#include "core/FixedTimestepLoop.hpp"
#include "core/Logger.hpp"
#include "jobs/JobSystem.hpp"
#include "physics/Systems.hpp"
#include "physics/CollisionSystem.hpp"
#include "physics/Components.hpp"
#include "ecs/World.hpp"

#include <atomic>
#include <algorithm>
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
    (void)t1; (void)t2;

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
    (void)ecsTransform; (void)ecsFetched;

    // Parallel Collision Detection test
    {
        physics::CollisionSystem collisionSystem;
        const size_t count = 200; // Enough to trigger parallel path
        std::vector<physics::AABBComponent> aabbs(count);
        std::vector<physics::CollisionEvent> events;

        // Place all AABBs at the same location so they all collide
        for(size_t i=0; i<count; ++i) {
            aabbs[i] = {0.0f, 0.0f, 1.0f, 1.0f};
        }

        collisionSystem.Detect(aabbs, events, &jobSystem);

        // Expected collisions: N * (N-1) / 2
        const size_t expected = count * (count - 1) / 2;
        if (events.size() != expected) {
            std::cerr << "Parallel collision test failed: expected " << expected << ", got " << events.size() << std::endl;
            return 1;
        }
    }

    // Collision Resolution test
    {
        physics::CollisionSystem collisionSystem;
        physics::CollisionResolutionSystem resolutionSystem;
        
        std::vector<physics::TransformComponent> resTransforms(2);
        std::vector<physics::RigidBodyComponent> resBodies(2);
        std::vector<physics::AABBComponent> aabbs(2);
        std::vector<physics::CollisionEvent> events;

        // Body A: at (0,0), moving right
        resTransforms[0] = {0.0f, 0.0f};
        resBodies[0] = {1.0f, 0.0f, 1.0f, 1.0f, 1.0f}; // vx=1, mass=1, rest=1 (elastic)
        aabbs[0] = {0.0f, 0.0f, 1.0f, 1.0f};

        // Body B: at (0.5, 0), moving left (overlapping A)
        resTransforms[1] = {0.5f, 0.0f};
        resBodies[1] = {-1.0f, 0.0f, 1.0f, 1.0f, 1.0f}; // vx=-1, mass=1, rest=1
        aabbs[1] = {0.5f, 0.0f, 1.5f, 1.0f};

        // Detect
        collisionSystem.Detect(aabbs, events);
        
        if (events.empty()) {
            std::cerr << "Collision Resolution test failed: No collision detected" << std::endl;
            return 1;
        }

        // Resolve
        resolutionSystem.Resolve(events, resTransforms, resBodies);

        // Check velocities (should bounce back)
        // Elastic collision of equal mass: velocities are exchanged.
        // A should become -1, B should become 1.
        if (resBodies[0].vx >= 0.0f || resBodies[1].vx <= 0.0f) {
             std::cerr << "Collision Resolution test failed: Velocities not resolved correctly. "
                       << "A.vx=" << resBodies[0].vx << ", B.vx=" << resBodies[1].vx << std::endl;
             return 1;
        }
    }

    // Broadphase Verification Test (Spatial Hash vs Brute Force)
    {
        physics::CollisionSystem collisionSystem;
        const size_t count = 500;
        std::vector<physics::AABBComponent> aabbs(count);
        
        // Generate random AABBs in a 100x100 area
        // Simple LCG for determinism
        uint32_t seed = 12345;
        auto randFloat = [&](float min, float max) {
            seed = seed * 1664525 + 1013904223;
            float f = static_cast<float>(seed) / static_cast<float>(0xFFFFFFFF);
            return min + f * (max - min);
        };

        for(size_t i=0; i<count; ++i) {
            float x = randFloat(0.0f, 100.0f);
            float y = randFloat(0.0f, 100.0f);
            float w = randFloat(0.5f, 2.0f);
            float h = randFloat(0.5f, 2.0f);
            aabbs[i] = {x, y, x + w, y + h};
        }

        std::vector<physics::CollisionEvent> eventsParallel;
        std::vector<physics::CollisionEvent> eventsSerial;

        // Run Parallel (Spatial Hash)
        collisionSystem.Detect(aabbs, eventsParallel, &jobSystem);

        // Run Serial (Brute Force) - pass nullptr for jobSystem
        collisionSystem.Detect(aabbs, eventsSerial, nullptr);

        if (eventsParallel.size() != eventsSerial.size()) {
             std::cerr << "Broadphase verification failed: Size mismatch. Parallel=" 
                       << eventsParallel.size() << ", Serial=" << eventsSerial.size() << std::endl;
             return 1;
        }

        // Sort and compare
        auto sortFn = [](const physics::CollisionEvent& a, const physics::CollisionEvent& b) {
            if (a.indexA != b.indexA) return a.indexA < b.indexA;
            return a.indexB < b.indexB;
        };
        std::sort(eventsParallel.begin(), eventsParallel.end(), sortFn);
        std::sort(eventsSerial.begin(), eventsSerial.end(), sortFn);

        for(size_t i=0; i<eventsParallel.size(); ++i) {
            if (eventsParallel[i].indexA != eventsSerial[i].indexA || 
                eventsParallel[i].indexB != eventsSerial[i].indexB) {
                std::cerr << "Broadphase verification failed: Event mismatch at index " << i << std::endl;
                return 1;
            }
        }
    }

    // ECS Physics Integration Test
    {
        ecs::World ecsPhysicsWorld;
        // We can reuse the existing jobSystem from main scope
        auto ecsPhysicsSystem = std::make_unique<physics::PhysicsIntegrationSystem>();
        ecsPhysicsSystem->SetJobSystem(&jobSystem);
        
        // Create entities
        const size_t count = 1000;
        for(size_t i=0; i<count; ++i) {
            auto entity = ecsPhysicsWorld.CreateEntity();
            ecsPhysicsWorld.AddComponent<physics::TransformComponent>(entity, physics::TransformComponent{0.0f, 0.0f});
            ecsPhysicsWorld.AddComponent<physics::RigidBodyComponent>(entity, physics::RigidBodyComponent{1.0f, 0.0f}); // vx=1
        }

        ecsPhysicsWorld.AddSystem(std::move(ecsPhysicsSystem));

        // Update
        ecsPhysicsWorld.Update(1.0f);

        // Verify
        auto* tfStorage = ecsPhysicsWorld.GetStorage<physics::TransformComponent>();
        auto& tfs = tfStorage->GetData();
        
        // Check a few samples to ensure parallel update worked
        if (tfs.empty()) {
             std::cerr << "ECS Physics test failed: No transforms found" << std::endl;
             return 1;
        }

        for(const auto& tf : tfs) {
            // v += a * dt  => vy = -9.81
            // p += v * dt  => y = -9.81
            if (std::abs(tf.x - 1.0f) > 0.001f || std::abs(tf.y - (-9.81f)) > 0.001f) {
                std::cerr << "ECS Physics test failed: Incorrect integration. "
                          << "x=" << tf.x << " (expected 1.0), y=" << tf.y << " (expected -9.81)" << std::endl;
                return 1;
            }
        }
    }

    // Full Physics Pipeline Test (PhysicsSystem)
    {
        ecs::World world;
        auto fullPhysicsSystem = std::make_unique<physics::PhysicsSystem>();
        fullPhysicsSystem->SetJobSystem(&jobSystem);
        world.AddSystem(std::move(fullPhysicsSystem));

        // Entity A: moving right
        auto e1 = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(e1, physics::TransformComponent{0.0f, 0.0f});
        world.AddComponent<physics::RigidBodyComponent>(e1, physics::RigidBodyComponent{1.0f, 0.0f}); // vx=1
        world.AddComponent<physics::AABBComponent>(e1, physics::AABBComponent{0.0f, 0.0f, 1.0f, 1.0f});

        // Entity B: moving left, colliding path
        auto e2 = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(e2, physics::TransformComponent{1.5f, 0.0f});
        world.AddComponent<physics::RigidBodyComponent>(e2, physics::RigidBodyComponent{-1.0f, 0.0f}); // vx=-1
        world.AddComponent<physics::AABBComponent>(e2, physics::AABBComponent{1.5f, 0.0f, 2.5f, 1.0f});

        // Step 1: Move them closer (Integration)
        // t=0: A at 0, B at 1.5. Distance 1.5. No overlap (A max 1, B min 1.5).
        // Update 0.5s: A -> 0.5, B -> 1.0.
        // AABB A: 0.5-1.5. AABB B: 1.0-2.0. Overlap!
        world.Update(0.5f);

        // Check if collision was resolved.
        // If resolved, velocities should have flipped (elastic collision).
        auto* b1 = world.GetComponent<physics::RigidBodyComponent>(e1);
        auto* b2 = world.GetComponent<physics::RigidBodyComponent>(e2);

        if (b1->vx >= 0.0f || b2->vx <= 0.0f) {
             std::cerr << "Physics Pipeline test failed: Velocities not resolved. "
                       << "A.vx=" << b1->vx << ", B.vx=" << b2->vx << std::endl;
             return 1;
        }
    }

    logger.Info("[PASS] Core and Jobs self-tests");
    std::cout << "All self-tests passed.\n";
    return 0;
}
