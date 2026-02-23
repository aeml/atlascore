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
#include <cmath>
#include <iostream>

namespace simlab
{
    class WreckingBallScenario : public IScenario
    {
    public:
        void Setup(ecs::World& world) override
        {
            m_renderer = std::make_unique<ascii::TextRenderer>(80, 30);
            
            physics::EnvironmentForces env;
            env.gravityY = -15.0f;
            env.drag = 0.01f;
            
            auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
            physics::PhysicsSettings settings;
            settings.substeps = 16;
            settings.constraintIterations = 16; // High for stable chains
            physicsSystem->SetSettings(settings);
            physicsSystem->SetEnvironment(env);
            physicsSystem->SetJobSystem(&m_jobSystem);
            world.AddSystem(std::move(physicsSystem));

            // Floor
            auto floor = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(floor, 0.0f, -10.0f, 0.0f);
            auto& fb = world.AddComponent<physics::RigidBodyComponent>(floor);
            fb.mass = 0.0f; fb.invMass = 0.0f;
            world.AddComponent<physics::AABBComponent>(floor, -40.0f, -11.0f, 40.0f, -9.0f);

            // Wall of boxes
            float boxSize = 1.5f;
            for (int y = 0; y < 8; ++y)
            {
                for (int x = 0; x < 6; ++x)
                {
                    auto box = world.CreateEntity();
                    const float xPos = 5.0f + x * boxSize;
                    const float yPos = -9.0f + y * boxSize + boxSize / 2;
                    world.AddComponent<physics::TransformComponent>(box, xPos, yPos, 0.0f);
                    auto& b = world.AddComponent<physics::RigidBodyComponent>(box);
                    b.mass = 1.0f; b.invMass = 1.0f; b.friction = 0.6f;
                    world.AddComponent<physics::AABBComponent>(
                        box,
                        xPos - boxSize / 2,
                        yPos - boxSize / 2,
                        xPos + boxSize / 2,
                        yPos + boxSize / 2);
                    physics::ConfigureBoxInertia(b, boxSize, boxSize);
                }
            }

            // Wrecking Ball Chain
            ecs::EntityId prevLink = world.CreateEntity(); // Anchor
            world.AddComponent<physics::TransformComponent>(prevLink, -10.0f, 10.0f, 0.0f);
            auto& ab = world.AddComponent<physics::RigidBodyComponent>(prevLink);
            ab.mass = 0.0f; ab.invMass = 0.0f;

            int chainLen = 10;
            float linkLen = 1.5f;
            for (int i = 0; i < chainLen; ++i)
            {
                auto link = world.CreateEntity();
                world.AddComponent<physics::TransformComponent>(link, -10.0f + (i+1)*1.0f, 10.0f - (i+1)*1.0f, 0.0f); // Initial swing pos
                auto& lb = world.AddComponent<physics::RigidBodyComponent>(link);
                
                if (i == chainLen - 1) {
                    // Heavy Ball
                    lb.mass = 50.0f; lb.invMass = 1.0f/50.0f;
                    world.AddComponent<physics::CircleColliderComponent>(link, 2.0f);
                    physics::ConfigureCircleInertia(lb, 2.0f);
                } else {
                    // Chain link
                    lb.mass = 0.5f; lb.invMass = 2.0f;
                    world.AddComponent<physics::CircleColliderComponent>(link, 0.2f);
                }

                auto& joint = world.AddComponent<physics::DistanceJointComponent>(link);
                joint.entityA = prevLink;
                joint.entityB = link;
                joint.targetDistance = linkLen;
                joint.compliance = 0.0f; // Rigid chain

                prevLink = link;
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
            
            // Draw Floor
            for(int x=0; x<80; ++x) m_renderer->Put(x, 29, '#');

            // Draw Entities
            world.ForEach<physics::TransformComponent>([&](ecs::EntityId id, physics::TransformComponent& t) {
                // Map -20..20 to 0..80
                int sx = static_cast<int>((t.x + 20.0f) * 2.0f);
                int sy = static_cast<int>(30.0f - (t.y + 10.0f));

                if (sx >= 0 && sx < 80 && sy >= 0 && sy < 30)
                {
                    char c = '*';
                    if (world.GetComponent<physics::AABBComponent>(id)) c = '#'; // Box
                    if (world.GetComponent<physics::DistanceJointComponent>(id)) c = '.'; // Chain
                    if (auto* rb = world.GetComponent<physics::RigidBodyComponent>(id)) {
                        if (rb->mass > 10.0f) c = 'O'; // Wrecking ball
                    }
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

    std::unique_ptr<IScenario> CreateWreckingBallScenario()
    {
        return std::make_unique<WreckingBallScenario>();
    }
}
