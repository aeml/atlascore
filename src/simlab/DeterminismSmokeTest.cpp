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

#include "core/Logger.hpp"
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "ascii/TextRenderer.hpp"
#include "jobs/JobSystem.hpp"

#include <vector>
#include <iostream>
#include <fstream>

namespace simlab
{
    namespace
    {
        struct FallDemoConfig
        {
            enum class HitPolicy
            {
                Despawn,
                Bounce
            };

            enum class EdgePolicy
            {
                Despawn,
                Ignore
            };

            float     worldMinX  = -10.0f;
            float     worldMaxX  =  10.0f;
            float     worldMinY  = -10.0f;
            float     worldMaxY  =  30.0f;
            HitPolicy onBottom   = HitPolicy::Despawn;
            EdgePolicy onEdges   = EdgePolicy::Despawn;
            float     spawnSpreadX = 8.0f;   // horizontal spread around center
        };

        static int WorldYToRow(float y, int height, const FallDemoConfig& cfg)
        {
            float span = cfg.worldMaxY - cfg.worldMinY;
            if (span <= 0.0f) span = 1.0f;
            float norm = (y - cfg.worldMinY) / span;
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;
            int row = height - 2 - static_cast<int>(norm * (height - 4));
            if (row < 1) row = 1;
            if (row > height - 2) row = height - 2;
            return row;
        }

        class DeterminismSmokeTest final : public IScenario
        {
        public:
            void Setup(ecs::World& world) override
            {
                // Route simlab logs to a dedicated file so they don't
                // interfere with the ASCII renderer output on stdout.
                auto logFile = std::make_shared<std::ofstream>("simlab_smoke.log", std::ios::app);
                if (logFile->is_open())
                {
                    m_logger.SetOutput(logFile);
                }

                m_logger.Info("[simlab] Setup determinism smoke test (ECS falling bodies)");
                m_width = 80;
                m_height = 24;
                m_renderer = std::make_unique<ascii::TextRenderer>(m_width, m_height);
                if (simlab::IsHeadlessRendering())
                {
                    m_renderer->SetHeadless(true);
                }
                m_cfg.worldMinX = -10.0f;
                m_cfg.worldMaxX =  10.0f;
                m_cfg.worldMinY = -10.0f;
                m_cfg.worldMaxY =  30.0f;
                m_cfg.onBottom  = FallDemoConfig::HitPolicy::Despawn;
                m_cfg.onEdges   = FallDemoConfig::EdgePolicy::Despawn;
                // Configure a gentle smoke-like environment: light gravity, sideways wind, some drag.
                physics::EnvironmentForces env;
                env.gravityY = -2.0f;   // slow fall
                env.windX    =  1.5f;   // drift to the right
                env.windY    =  0.0f;
                env.drag     =  0.2f;   // keep velocities bounded
                m_physics.SetEnvironment(env);
                m_physics.SetJobSystem(&m_jobSystem);
                // Clear screen and draw a simple title banner on first setup
                m_renderer->Clear(' ');
                const char* title = "ECS falling bodies";
                const int titleLen = 19;
                int startX = (m_width - titleLen) / 2;
                if (startX < 0) startX = 0;
                for (int i = 0; i < titleLen && (startX + i) < m_width; ++i)
                {
                    m_renderer->Put(startX + i, 0, title[i]);
                }
                m_renderer->PresentDiff(std::cout);
                // Add systems (wire ECS physics to the configured environment).
                auto physicsSystem = std::make_unique<physics::PhysicsSystem>();
                physicsSystem->SetEnvironment(env);
                physicsSystem->SetJobSystem(&m_jobSystem);
                world.AddSystem(std::move(physicsSystem));
                
                // Create initial demo entities with components; we will maintain
                // a target particle count over time.
                const int initialCount = 8;
                for (int i = 0; i < initialCount; ++i)
                {
                    SpawnSmokeParticle(world, static_cast<float>(i));
                }
            }

            void Update(ecs::World& world, float dt) override
            {
#ifdef ATLASCORE_DEBUG_SIMLAB
                m_logger.Info("[smoke] Step begin");
#endif

                // Ensure we maintain a target number of particles that are
                // inside the world-space smoke volume.
                const std::size_t targetParticles = 32;
                auto countAliveParticles = [&](ecs::World& w) {
                    std::size_t count = 0;
                    w.ForEach<physics::AABBComponent>([&](ecs::EntityId /*id*/, physics::AABBComponent& box)
                    {
                        if (box.minY >= m_cfg.worldMinY && box.minY <= m_cfg.worldMaxY)
                        {
                            ++count;
                        }
                    });
                    return count;
                };

                std::size_t currentParticles = countAliveParticles(world);

                // Handle bodies that hit the bottom or leave the world according to policy.
                std::vector<ecs::EntityId> toDespawn;
                world.ForEach<physics::TransformComponent>([&](ecs::EntityId id, physics::TransformComponent& t)
                {
                    auto* rb   = world.GetComponent<physics::RigidBodyComponent>(id);
                    auto* aabb = world.GetComponent<physics::AABBComponent>(id);
                    if (!rb || !aabb)
                    {
                        return;
                    }

                    // World-space bottom: once the AABB's minY drops below worldMinY,
                    // treat it as having left the smoke volume.
                    const float y       = aabb->minY;

                    const bool hitBottom = (y < m_cfg.worldMinY);

                    if (hitBottom)
                    {
#ifdef ATLASCORE_DEBUG_SIMLAB
                        m_logger.Info("[smoke] entity hit bottom; applying policy (id=" + std::to_string(id) + ")");
#endif
                        switch (m_cfg.onBottom)
                        {
                        case FallDemoConfig::HitPolicy::Despawn:
                        {
                            toDespawn.push_back(id);
                            break;
                        }
                        case FallDemoConfig::HitPolicy::Bounce:
                        {
                            rb->vy = -rb->vy * 0.7f;
                            const float half = 0.5f;
                            t.y = m_cfg.worldMinY + half; // clamp slightly above ground
                            aabb->minY = t.y - half;
                            aabb->maxY = t.y + half;
                            break;
                        }
                        }
                    }
                });

                // Destroy entities that hit the bottom; new ones will be spawned
                // below to keep the effect continuous.
                for (ecs::EntityId id : toDespawn)
                {
                    world.DestroyEntity(id);
                }

                // Re-fill the smoke volume back up to the target count so that
                // the effect never "dries out" after particles leave the world.
                currentParticles = countAliveParticles(world);
                while (currentParticles < targetParticles)
                {
                    SpawnSmokeParticle(world, static_cast<float>(currentParticles));
                    ++currentParticles;
                }

                // Integrate physics for non-ECS demo bodies as well.
                // (For now they are managed manually; this still shows environment behavior.)
                (void)dt; // dt is currently driven externally; integration is handled by ECS systems.
            }

            void Render(ecs::World& world, std::ostream& out) override
            {
                if (!m_renderer)
                {
                    return;
                }

                // Visual mode: draw current bodies with the text renderer.
                m_renderer->Clear(' ');

                // Draw each AABB as a column of '#'
                std::size_t bodyCount = 0;
                world.ForEach<physics::AABBComponent>([&](ecs::EntityId /*id*/, physics::AABBComponent& box)
                {
                    // Use box center X and minY to approximate screen position
                    const float centerX = 0.5f * (box.minX + box.maxX);
                    const float y = box.minY;

                    int sx = static_cast<int>((centerX + 10.0f) / 20.0f * (m_width - 2));
                    if (sx < 1) sx = 1;
                    if (sx > m_width - 2) sx = m_width - 2;

                    int sy = WorldYToRow(y, m_height, m_cfg);

                    m_renderer->Put(sx, sy, '#');
                    ++bodyCount;
                });

                // Draw a simple ground line near the bottom
                for (int x = 0; x < m_width; ++x)
                {
                    m_renderer->Put(x, m_height - 2, '-');
                }

                m_renderer->PresentDiff(out);

#ifdef ATLASCORE_DEBUG_SIMLAB
                m_logger.Info("[smoke] debug step; bodies=" + std::to_string(bodyCount));
#endif
            }

        private:
            int                                       m_width{80};
            int                                       m_height{24};
            std::unique_ptr<ascii::TextRenderer>      m_renderer;
            jobs::JobSystem                           m_jobSystem;
            physics::PhysicsIntegrationSystem         m_physics;
            FallDemoConfig                            m_cfg;
            core::Logger                              m_logger;

            void SpawnSmokeParticle(ecs::World& world, float seed)
            {
                // Simple procedural horizontal spread: center around 0 with some noise.
                const float x = (seed * 1.37f - 0.5f) * m_cfg.spawnSpreadX;
                const float y = m_cfg.worldMaxY;

                auto e = world.CreateEntity();
                world.AddComponent<physics::TransformComponent>(e, physics::TransformComponent{x, y});
                world.AddComponent<physics::RigidBodyComponent>(e, physics::RigidBodyComponent{0.0f, -3.0f});

                auto* t = world.GetComponent<physics::TransformComponent>(e);
                const float half = 0.5f;
                physics::AABBComponent box{t->x - half, t->y - half, t->x + half, t->y + half};
                world.AddComponent<physics::AABBComponent>(e, box);
            }
        };
    }

    std::unique_ptr<IScenario> CreateDeterminismSmokeTest()
    {
        return std::make_unique<DeterminismSmokeTest>();
    }

    // Auto-register
    static ScenarioAutoRegister g_reg_smoke{"smoke", "ECS falling bodies smoke test", &CreateDeterminismSmokeTest};
}
