/*
 * Copyright (C) 2025 aeml
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "simlab/Scenario.hpp"
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"
#include "ascii/TextRenderer.hpp"
#include <vector>
#include <random>
#include <iostream>

namespace simlab
{
    class ParticleFluidScenario : public IScenario
    {
    public:
        void Setup(ecs::World& world) override
        {
            m_renderer = std::make_unique<ascii::TextRenderer>(80, 40);
            
            physics::EnvironmentForces env;
            env.gravityY = -9.81f;
            
            auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
            physics::PhysicsSettings settings;
            settings.substeps = 8; // Increased substeps for stability
            physicsSystem->SetSettings(settings);
            physicsSystem->SetEnvironment(env);
            physicsSystem->SetJobSystem(&m_jobSystem);
            world.AddSystem(std::move(physicsSystem));

            // Container (Closed box)
            // Visible range: X[-20, 20], Y[-15, 25]
            // Walls made very thick (100.0f) to prevent tunneling
            // Inner faces at: Left -18.5, Right 18.5, Bottom -13.5, Top 23.5
            CreateWall(world, -68.5f, 5.0f, 100.0f, 40.0f); // Left
            CreateWall(world, 68.5f, 5.0f, 100.0f, 40.0f);  // Right
            CreateWall(world, 0.0f, -63.5f, 40.0f, 100.0f); // Bottom
            CreateWall(world, 0.0f, 73.5f, 40.0f, 100.0f);  // Top

            // Particles
            std::mt19937 rng(123);
            std::uniform_real_distribution<float> distX(-15.0f, 15.0f);
            std::uniform_real_distribution<float> distY(-5.0f, 15.0f);

            for (int i = 0; i < 100; ++i)
            {
                float x = distX(rng);
                float y = distY(rng);
                auto p = world.CreateEntity();
                world.AddComponent<physics::TransformComponent>(p, x, y, 0.0f);
                auto& b = world.AddComponent<physics::RigidBodyComponent>(p);
                b.mass = 0.1f; b.invMass = 10.0f; b.restitution = 0.9f; b.friction = 0.0f; // High restitution for bouncing
                b.lastX = x;
                b.lastY = y;
                world.AddComponent<physics::CircleColliderComponent>(p, 0.3f);
                // Add AABB for collision detection
                world.AddComponent<physics::AABBComponent>(p, x - 0.3f, y - 0.3f, x + 0.3f, y + 0.3f);
            }
        }

        void CreateWall(ecs::World& world, float x, float y, float w, float h)
        {
            auto wall = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(wall, x, y, 0.0f);
            auto& b = world.AddComponent<physics::RigidBodyComponent>(wall);
            b.mass = 0.0f; b.invMass = 0.0f;
            // AABB must be in world space. Since static bodies don't update AABB, we must set it correctly here.
            world.AddComponent<physics::AABBComponent>(wall, x - w/2, y - h/2, x + w/2, y + h/2);
        }

        void Update(ecs::World& world, float dt) override
        {
            world.Update(dt);
        }

        void Render(ecs::World& world, std::ostream& out) override
        {
            m_renderer->Clear();
            
            world.ForEach<physics::TransformComponent>([&](ecs::EntityId id, physics::TransformComponent& t) {
                // Map -20..20 to 0..80
                int sx = static_cast<int>((t.x + 20.0f) * 2.0f);
                int sy = static_cast<int>(40.0f - (t.y + 15.0f));

                if (sx >= 0 && sx < 80 && sy >= 0 && sy < 40)
                {
                    char c = '.';
                    if (world.GetComponent<physics::AABBComponent>(id)) c = '#';
                    m_renderer->Put(sx, sy, c);
                }
            });

            m_renderer->SetHeadless(simlab::IsHeadlessRendering());
            m_renderer->PresentDiff(out);
        }

    private:
        std::unique_ptr<ascii::TextRenderer> m_renderer;
        jobs::JobSystem m_jobSystem;
    };

    std::unique_ptr<IScenario> CreateParticleFluidScenario()
    {
        return std::make_unique<ParticleFluidScenario>();
    }
}
