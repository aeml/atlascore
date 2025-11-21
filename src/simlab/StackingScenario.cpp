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
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace simlab
{
    namespace
    {
        struct StackingConfig
        {
            float worldMinX = -10.0f;
            float worldMaxX = 10.0f;
            float worldMinY = -5.0f;
            float worldMaxY = 15.0f;
        };

        // Helper to map world coordinates to screen coordinates
        void WorldToScreen(float wx, float wy, int sw, int sh, const StackingConfig& cfg, int& sx, int& sy)
        {
            float spanX = cfg.worldMaxX - cfg.worldMinX;
            float spanY = cfg.worldMaxY - cfg.worldMinY;
            
            float normX = (wx - cfg.worldMinX) / spanX;
            float normY = (wy - cfg.worldMinY) / spanY;
            
            sx = static_cast<int>(normX * (sw - 1));
            sy = (sh - 1) - static_cast<int>(normY * (sh - 1));
        }
    }

    class StackingScenario : public IScenario
    {
    public:
        void Setup(ecs::World& world) override
        {
            auto logFile = std::make_shared<std::ofstream>("simlab_stack.log");
            m_logger.SetOutput(logFile);
            m_logger.Info("Stacking Scenario Setup");

            m_width = 80;
            m_height = 24;
            m_renderer = std::make_unique<ascii::TextRenderer>(m_width, m_height);
            
            physics::EnvironmentForces env;
            env.gravityY = -9.81f;
            env.drag = 0.05f;
            
            auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
            physics::PhysicsSettings settings;
            settings.substeps = 24;
            settings.positionIterations = 32;
            settings.velocityIterations = 18;
            settings.constraintIterations = 12;
            settings.correctionPercent = 0.3f;
            settings.maxPositionCorrection = 0.1f;
            physicsSystem->SetSettings(settings);
            physicsSystem->SetEnvironment(env);
            physicsSystem->SetJobSystem(&m_jobSystem);
            world.AddSystem(std::move(physicsSystem));

            // Floor
            CreateBox(world, 0.0f, -4.0f, 18.0f, 1.0f, 0.0f);

            // Pyramid
            float boxSize = 1.2f;
            int rows = 6;
            float startY = -3.0f;

            for (int row = 0; row < rows; ++row)
            {
                int cols = rows - row;
                float startX = -(cols - 1) * boxSize * 0.5f;
                for (int col = 0; col < cols; ++col)
                {
                    float x = startX + col * boxSize;
                    float y = startY + row * boxSize;
                    CreateBox(world, x, y, boxSize * 0.9f, boxSize * 0.9f, 1.0f);
                }
            }
        }

        void Update(ecs::World& world, float dt) override
        {
            (void)dt;
            
            // Debug logging for the top box (last created entity usually)
            static int frame = 0;
            if (frame++ % 10 == 0) {
                world.ForEach<physics::RigidBodyComponent>([&](ecs::EntityId id, physics::RigidBodyComponent& rb) {
                    if (rb.invMass > 0.0f && id > 15) { // Just pick one high up
                         auto* tf = world.GetComponent<physics::TransformComponent>(id);
                         auto* aabb = world.GetComponent<physics::AABBComponent>(id);
                         if (tf && aabb) {
                             m_logger.Info("Frame " + std::to_string(frame) + " Entity " + std::to_string(id) + 
                                           ": Pos(" + std::to_string(tf->x) + ", " + std::to_string(tf->y) + 
                                           ") Vel(" + std::to_string(rb.vx) + ", " + std::to_string(rb.vy) + 
                                           ") AABB(" + std::to_string(aabb->minY) + ", " + std::to_string(aabb->maxY) + ")");
                         }
                    }
                });
            }
        }

        void Render(ecs::World& world, std::ostream& out) override
        {
            if (!m_renderer) return;
            m_renderer->Clear(' ');

            DrawGrid();

            world.ForEach<physics::AABBComponent>([&](ecs::EntityId, physics::AABBComponent& box) {
                DrawBox(box);
            });

            m_renderer->PresentDiff(out);
        }

    private:
        jobs::JobSystem m_jobSystem;
        std::unique_ptr<ascii::TextRenderer> m_renderer;
        StackingConfig m_cfg;
        core::Logger m_logger;
        int m_width{80};
        int m_height{24};

        void DrawGrid()
        {
            float cellSize = 2.0f;
            // Vertical lines
            // Align to cell size
            float startX = std::floor(m_cfg.worldMinX / cellSize) * cellSize;
            for (float x = startX; x <= m_cfg.worldMaxX; x += cellSize)
            {
                int sx, sy1, sy2;
                int dummy;
                WorldToScreen(x, m_cfg.worldMinY, m_width, m_height, m_cfg, sx, sy1);
                WorldToScreen(x, m_cfg.worldMaxY, m_width, m_height, m_cfg, dummy, sy2);
                
                if (sx >= 0 && sx < m_width)
                {
                     m_renderer->DrawLine(sx, std::max(0, sy2), sx, std::min(m_height - 1, sy1), '.');
                }
            }
            // Horizontal lines
            float startY = std::floor(m_cfg.worldMinY / cellSize) * cellSize;
            for (float y = startY; y <= m_cfg.worldMaxY; y += cellSize)
            {
                int sy, sx1, sx2;
                int dummy;
                WorldToScreen(m_cfg.worldMinX, y, m_width, m_height, m_cfg, sx1, sy);
                WorldToScreen(m_cfg.worldMaxX, y, m_width, m_height, m_cfg, sx2, dummy);
                
                if (sy >= 0 && sy < m_height)
                {
                    m_renderer->DrawLine(std::max(0, sx1), sy, std::min(m_width - 1, sx2), sy, '.');
                }
            }
        }

        void CreateBox(ecs::World& world, float x, float y, float w, float h, float mass)
        {
            auto e = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{x, y, 0.0f});
            float invMass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
            physics::RigidBodyComponent rb{};
            rb.vx = 0.0f;
            rb.vy = 0.0f;
            rb.lastX = x;
            rb.lastY = y;
            rb.lastAngle = 0.0f;
            rb.mass = mass;
            rb.invMass = invMass;
            rb.restitution = 0.05f;
            rb.friction = (mass > 0.0f) ? 0.8f : 1.0f;
            rb.angularFriction = 0.4f;
            rb.angularDrag = 0.02f;
            physics::ConfigureBoxInertia(rb, w, h);
            world.AddComponent<physics::RigidBodyComponent>(e, rb);
            world.AddComponent<physics::AABBComponent>(e, physics::AABBComponent{x - w/2, y - h/2, x + w/2, y + h/2});
        }

        void DrawBox(const physics::AABBComponent& box)
        {
            int x1, y1, x2, y2;
            WorldToScreen(box.minX, box.maxY, m_width, m_height, m_cfg, x1, y1); // Top-Left
            WorldToScreen(box.maxX, box.minY, m_width, m_height, m_cfg, x2, y2); // Bottom-Right

            // Clamp
            x1 = std::max(0, std::min(x1, m_width - 1));
            x2 = std::max(0, std::min(x2, m_width - 1));
            y1 = std::max(0, std::min(y1, m_height - 1));
            y2 = std::max(0, std::min(y2, m_height - 1));

            for (int y = y1; y <= y2; ++y)
            {
                for (int x = x1; x <= x2; ++x)
                {
                    char c = '#';
                    if (y == y1 || y == y2) c = '-';
                    else if (x == x1 || x == x2) c = '|';
                    
                    // Corners
                    if ((x == x1 || x == x2) && (y == y1 || y == y2)) c = '+';

                    m_renderer->Put(x, y, c);
                }
            }
        }
    };

    std::unique_ptr<IScenario> CreateStackingScenario()
    {
        return std::make_unique<StackingScenario>();
    }
}
