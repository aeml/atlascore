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
#include "core/Logger.hpp"
#include <vector>
#include <cmath>
#include <random>
#include <iostream>

namespace simlab
{
    namespace
    {
        // Custom system for N-body gravity
        class PlanetaryGravitySystem : public ecs::ISystem
        {
        public:
            void Update(ecs::World& world, float dt) override
            {
                // Simple approach: Apply force towards center (0,0) for all dynamic bodies
                // Or true N-body if we want to be fancy, but O(N^2) is slow for many bodies without optimization.
                // Let's do a massive central attractor at (0,0).
                
                const float G = 100.0f;
                const float centerMass = 1000.0f;

                world.ForEach<physics::TransformComponent>([&](ecs::EntityId id, physics::TransformComponent& t) {
                    auto* body = world.GetComponent<physics::RigidBodyComponent>(id);
                    if (body && body->invMass > 0.0f)
                    {
                        float dx = 0.0f - t.x;
                        float dy = 0.0f - t.y;
                        float distSq = dx*dx + dy*dy;
                        float dist = std::sqrt(distSq);
                        
                        if (dist > 0.1f)
                        {
                            float force = (G * centerMass * body->mass) / distSq;
                            float fx = (dx / dist) * force;
                            float fy = (dy / dist) * force;

                            // Apply force (modify velocity directly for this simple integrator)
                            body->vx += (fx * body->invMass) * dt;
                            body->vy += (fy * body->invMass) * dt;
                        }
                    }
                });
            }
        };
    }

    class PlanetaryGravityScenario : public IScenario
    {
    public:
        void Setup(ecs::World& world) override
        {
            m_renderer = std::make_unique<ascii::TextRenderer>(80, 40);
            
            // Physics setup
            physics::EnvironmentForces env;
            env.gravityY = 0.0f; // No global gravity, only radial
            env.drag = 0.0f; // Space has no drag
            
            auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
            physics::PhysicsSettings settings;
            settings.substeps = 8;
            physicsSystem->SetSettings(settings);
            physicsSystem->SetEnvironment(env);
            physicsSystem->SetJobSystem(&m_jobSystem);
            world.AddSystem(std::move(physicsSystem));

            // Add custom gravity system
            world.AddSystem(std::make_unique<PlanetaryGravitySystem>());

            // Central Star (Static)
            auto star = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(star, 0.0f, 0.0f, 0.0f);
            auto& starBody = world.AddComponent<physics::RigidBodyComponent>(star);
            starBody.mass = 0.0f; // Static
            starBody.invMass = 0.0f;
            world.AddComponent<physics::CircleColliderComponent>(star, 2.0f);

            // Planets
            std::mt19937 rng(42);
            std::uniform_real_distribution<float> distAngle(0.0f, 6.28318f);
            std::uniform_real_distribution<float> distRadius(5.0f, 35.0f);
            std::uniform_real_distribution<float> distMass(0.5f, 2.0f);

            for (int i = 0; i < 100; ++i)
            {
                float angle = distAngle(rng);
                float r = distRadius(rng);
                float mass = distMass(rng);

                float x = std::cos(angle) * r;
                float y = std::sin(angle) * r;

                // Orbital velocity: v = sqrt(GM/r)
                // Perpendicular to radius
                float v = std::sqrt(100.0f * 1000.0f / r);
                float vx = -std::sin(angle) * v;
                float vy = std::cos(angle) * v;

                auto p = world.CreateEntity();
                world.AddComponent<physics::TransformComponent>(p, x, y, 0.0f);
                auto& body = world.AddComponent<physics::RigidBodyComponent>(p);
                body.mass = mass;
                body.invMass = 1.0f / mass;
                body.vx = vx;
                body.vy = vy;
                body.restitution = 0.8f;
                
                world.AddComponent<physics::CircleColliderComponent>(p, 0.5f);
                physics::ConfigureCircleInertia(body, 0.5f);
            }
        }

        void Update(ecs::World& world, float dt) override
        {
            (void)world;
            (void)dt;
        }

        void Render(ecs::World& world, std::ostream& out) override
        {
            m_renderer->Clear();
            
            // Draw Star
            m_renderer->DrawCircle(40, 20, 4, '@');

            // Draw Planets
            world.ForEach<physics::TransformComponent>([&](ecs::EntityId, physics::TransformComponent& t) {
                // Map -40..40 to 0..80
                int sx = static_cast<int>(t.x + 40.0f);
                int sy = static_cast<int>(20.0f - t.y * 0.5f); // Aspect ratio correction roughly
                
                if (sx >= 0 && sx < 80 && sy >= 0 && sy < 40)
                {
                    m_renderer->Put(sx, sy, 'o');
                }
            });

            m_renderer->SetHeadless(simlab::IsHeadlessRendering());
            m_renderer->PresentDiff(out);
        }

    private:
        std::unique_ptr<ascii::TextRenderer> m_renderer;
        jobs::JobSystem m_jobSystem;
    };

    std::unique_ptr<IScenario> CreatePlanetaryGravityScenario()
    {
        return std::make_unique<PlanetaryGravityScenario>();
    }
}
