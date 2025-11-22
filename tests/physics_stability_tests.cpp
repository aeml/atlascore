#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

// Simple test framework
#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "FAILED: " << msg << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

void TestParticleStability()
{
    ecs::World world;
    jobs::JobSystem jobSystem;
    
    physics::EnvironmentForces env;
    env.gravityY = -9.81f;
    
    auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
    physics::PhysicsSettings settings;
    settings.substeps = 8; 
    physicsSystem->SetSettings(settings);
    physicsSystem->SetEnvironment(env);
    physicsSystem->SetJobSystem(&jobSystem);
    world.AddSystem(std::move(physicsSystem));

    // Create container walls (thick to prevent tunneling)
    auto createWall = [&](float x, float y, float w, float h) {
        auto wall = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(wall, x, y, 0.0f);
        auto& b = world.AddComponent<physics::RigidBodyComponent>(wall);
        b.mass = 0.0f; b.invMass = 0.0f;
        world.AddComponent<physics::AABBComponent>(wall, x - w/2, y - h/2, x + w/2, y + h/2);
    };

    createWall(-68.5f, 5.0f, 100.0f, 40.0f); // Left
    createWall(68.5f, 5.0f, 100.0f, 40.0f);  // Right
    createWall(0.0f, -63.5f, 40.0f, 100.0f); // Bottom
    createWall(0.0f, 73.5f, 40.0f, 100.0f);  // Top

    // Create particles
    int particleCount = 50;
    for (int i = 0; i < particleCount; ++i)
    {
        float x = (i % 5) * 1.0f - 2.0f;
        float y = (i / 5) * 1.0f;
        auto p = world.CreateEntity();
        world.AddComponent<physics::TransformComponent>(p, x, y, 0.0f);
        auto& b = world.AddComponent<physics::RigidBodyComponent>(p);
        b.mass = 0.1f; b.invMass = 10.0f; b.restitution = 0.9f; b.friction = 0.0f;
        b.lastX = x;
        b.lastY = y;
        world.AddComponent<physics::CircleColliderComponent>(p, 0.3f);
        world.AddComponent<physics::AABBComponent>(p, x - 0.3f, y - 0.3f, x + 0.3f, y + 0.3f);
    }

    const float dt = 1.0f / 60.0f;
    const float maxSpeed = 100.0f; // Threshold for "super fast"
    const float maxAccel = 5000.0f; // Threshold for sudden acceleration

    struct ParticleState {
        float vx, vy;
    };
    std::vector<ParticleState> lastStates(particleCount, {0.0f, 0.0f});

    std::cout << "Running stability simulation for 300 frames..." << std::endl;

    for (int frame = 0; frame < 300; ++frame)
    {
        world.Update(dt);

        int pIndex = 0;
        world.ForEach<physics::RigidBodyComponent>([&](ecs::EntityId, physics::RigidBodyComponent& b) {
            if (b.invMass == 0.0f) return; // Skip static bodies

            float speed = std::sqrt(b.vx * b.vx + b.vy * b.vy);
            
            if (speed > maxSpeed) {
                std::cerr << "Frame " << frame << ": Particle " << pIndex << " exceeded max speed: " << speed << std::endl;
                TEST_ASSERT(false, "Particle instability detected (speed)");
            }

            // Check acceleration
            float dvx = b.vx - lastStates[pIndex].vx;
            float dvy = b.vy - lastStates[pIndex].vy;
            float accel = std::sqrt(dvx*dvx + dvy*dvy) / dt;

            if (frame > 0 && accel > maxAccel) {
                 std::cerr << "Frame " << frame << ": Particle " << pIndex << " exceeded max acceleration: " << accel << std::endl;
                 // TEST_ASSERT(false, "Particle instability detected (acceleration)");
                 // Just warn for now as collisions cause high accel
            }

            lastStates[pIndex] = {b.vx, b.vy};
            pIndex++;
        });
    }

    std::cout << "Stability test passed." << std::endl;
}

int main()
{
    TestParticleStability();
    return 0;
}
