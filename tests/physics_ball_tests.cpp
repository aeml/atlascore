#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"

#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    struct BallDesc
    {
        float x;
        float y;
        float radius;
        float mass;
        float vx;
        float vy;
        float restitution;
        float friction;
    };

    ecs::EntityId CreateBall(ecs::World& world, const BallDesc& desc)
    {
        auto e = world.CreateEntity();

        physics::TransformComponent tf{};
        tf.x = desc.x;
        tf.y = desc.y;
        tf.rotation = 0.0f;
        world.AddComponent<physics::TransformComponent>(e, tf);

        physics::RigidBodyComponent rb{};
        rb.vx = desc.vx;
        rb.vy = desc.vy;
        rb.lastX = desc.x;
        rb.lastY = desc.y;
        rb.lastAngle = tf.rotation;
        rb.mass = desc.mass;
        rb.invMass = desc.mass > 0.0f ? 1.0f / desc.mass : 0.0f;
        rb.restitution = desc.restitution;
        rb.friction = desc.friction;
        rb.angularFriction = 0.2f;
        rb.angularDrag = 0.01f;
        rb.angularVelocity = 0.0f;
        rb.torque = 0.0f;
        physics::ConfigureCircleInertia(rb, desc.radius);
        world.AddComponent<physics::RigidBodyComponent>(e, rb);

        const float r = desc.radius;
        physics::AABBComponent aabb{};
        aabb.minX = desc.x - r;
        aabb.maxX = desc.x + r;
        aabb.minY = desc.y - r;
        aabb.maxY = desc.y + r;
        world.AddComponent<physics::AABBComponent>(e, aabb);

        physics::CircleColliderComponent circle{};
        circle.radius = r;
        world.AddComponent<physics::CircleColliderComponent>(e, circle);

        return e;
    }

    void CreateStaticAabb(ecs::World& world, float minX, float minY, float maxX, float maxY)
    {
        auto e = world.CreateEntity();
        physics::TransformComponent tf{};
        tf.x = (minX + maxX) * 0.5f;
        tf.y = (minY + maxY) * 0.5f;
        tf.rotation = 0.0f;
        world.AddComponent<physics::TransformComponent>(e, tf);

        physics::RigidBodyComponent rb{};
        rb.mass = 0.0f;
        rb.invMass = 0.0f;
        rb.lastX = tf.x;
        rb.lastY = tf.y;
        rb.lastAngle = tf.rotation;
        rb.restitution = 0.0f;
        rb.friction = 1.0f;
        rb.inertia = 0.0f;
        rb.invInertia = 0.0f;
        world.AddComponent<physics::RigidBodyComponent>(e, rb);

        physics::AABBComponent aabb{};
        aabb.minX = minX;
        aabb.maxX = maxX;
        aabb.minY = minY;
        aabb.maxY = maxY;
        world.AddComponent<physics::AABBComponent>(e, aabb);
    }

    void StepWorld(ecs::World& world, float seconds, float dt, const std::function<void(int)>& perStep = {})
    {
        const int steps = static_cast<int>(std::ceil(seconds / dt));
        for (int i = 0; i < steps; ++i)
        {
            world.Update(dt);
            if (perStep)
            {
                perStep(i);
            }
        }
    }

    std::unique_ptr<physics::PhysicsSystem> CreatePhysicsSystem(const physics::EnvironmentForces& env)
    {
        auto system = std::make_unique<physics::PhysicsSystem>();
        system->SetEnvironment(env);
        return system;
    }

    void TestSideImpactTransfersVelocity()
    {
        ecs::World world;

        physics::EnvironmentForces env{};
        env.gravityY = 0.0f;
        env.drag = 0.0f;

        auto physicsSystem = CreatePhysicsSystem(env);
        world.AddSystem(std::move(physicsSystem));

        auto hitter = CreateBall(world, BallDesc{-2.0f, 0.0f, 0.5f, 1.0f, 6.0f, 0.0f, 0.95f, 0.0f});
        auto target = CreateBall(world, BallDesc{0.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.95f, 0.0f});

        StepWorld(world, 1.5f, 1.0f / 120.0f);

        auto* hitterBody = world.GetComponent<physics::RigidBodyComponent>(hitter);
        auto* targetBody = world.GetComponent<physics::RigidBodyComponent>(target);
        assert(hitterBody && targetBody);
        assert(hitterBody->vx < 2.0f);
        assert(targetBody->vx > 4.0f);
        assert(std::abs(targetBody->vy) < 0.5f);
    }

    void TestVerticalDropLaunchesCluster()
    {
        ecs::World world;

        physics::EnvironmentForces env{};
        env.gravityY = -9.81f;
        env.drag = 0.02f;

        auto physicsSystem = CreatePhysicsSystem(env);
        world.AddSystem(std::move(physicsSystem));

        CreateStaticAabb(world, -5.0f, -0.5f, 5.0f, 0.0f);

        std::vector<ecs::EntityId> base;
        const float baseRadius = 0.5f;
        for (float x : {-0.7f, 0.0f, 0.7f})
        {
            base.push_back(CreateBall(world, BallDesc{x, 0.5f, baseRadius, 1.0f, 0.0f, 0.0f, 0.4f, 0.2f}));
        }

        auto hammer = CreateBall(world, BallDesc{0.0f, 4.0f, baseRadius * 1.2f, 3.0f, 0.0f, -2.0f, 0.7f, 0.05f});
        (void)hammer;

        bool launched = false;
        StepWorld(world, 3.0f, 1.0f / 180.0f, [&](int) {
            for (auto id : base)
            {
                auto* rb = world.GetComponent<physics::RigidBodyComponent>(id);
                auto* tf = world.GetComponent<physics::TransformComponent>(id);
                if (rb && tf)
                {
                    const float speedSq = rb->vx * rb->vx + rb->vy * rb->vy;
                    if (speedSq > 0.5f * 0.5f || tf->y > 1.0f)
                    {
                        launched = true;
                    }
                }
            }
        });

        assert(launched);
    }

    void TestGlancingHitProducesAngularResponse()
    {
        ecs::World world;

        physics::EnvironmentForces env{};
        env.gravityY = 0.0f;
        env.drag = 0.0f;

        auto physicsSystem = CreatePhysicsSystem(env);
        world.AddSystem(std::move(physicsSystem));

        auto striker = CreateBall(world, BallDesc{-2.5f, -0.1f, 0.5f, 1.0f, 7.0f, 0.0f, 0.9f, 0.0f});
        auto stack = CreateBall(world, BallDesc{0.0f, 0.35f, 0.5f, 1.0f, 0.0f, 0.0f, 0.9f, 0.0f});

        StepWorld(world, 1.8f, 1.0f / 180.0f);

        auto* strikerRb = world.GetComponent<physics::RigidBodyComponent>(striker);
        auto* stackRb = world.GetComponent<physics::RigidBodyComponent>(stack);
        assert(strikerRb && stackRb);
        assert(stackRb->vy > 0.5f);
        assert(stackRb->vx > 1.0f);
        assert(strikerRb->vx < 6.0f);
    }
}

int main()
{
    TestSideImpactTransfersVelocity();
    TestVerticalDropLaunchesCluster();
    TestGlancingHitProducesAngularResponse();

    std::cout << "Physics ball tests passed\n";
    return 0;
}
