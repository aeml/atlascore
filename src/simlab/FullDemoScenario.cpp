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

// FullDemoScenario — exercises every AtlasCore subsystem in a single scene.
//
// Systems / features shown:
//   ECS        — CreateEntity, AddComponent, GetComponent, GetStorage,
//                ForEach<T>, View<T1,T2> (multi-component intersection)
//   Physics    — EnvironmentForces (gravity + drag), PhysicsSettings
//                (substeps, constraintIterations), ConfigureCircleInertia,
//                ConfigureBoxInertia, restitution, friction, angularDrag
//   Bodies     — static AABB floor/walls, static AABB platform,
//                dynamic AABB box tower, dynamic circle particles,
//                dynamic circle chain links + heavy pendulum ball
//   Joints     — DistanceJointComponent rigid chain (compliance = 0)
//   Custom sys — WindGustSystem: custom ISystem, alternating horizontal
//                impulse every kWindPeriod seconds
//   JobSystem  — m_jobs owned by scenario, passed to PhysicsSystem
//   ASCII      — TextRenderer with all 8 Color values,
//                DrawRect / DrawLine / DrawCircle / DrawEllipse /
//                FillEllipse / Put
//   Logger     — core::Logger for setup-time diagnostic messages

#include "simlab/Scenario.hpp"
#include "ecs/World.hpp"
#include "physics/Components.hpp"
#include "physics/Systems.hpp"
#include "jobs/JobSystem.hpp"
#include "ascii/TextRenderer.hpp"
#include "core/Logger.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace simlab
{
    // =========================================================================
    //  WindGustSystem — custom ISystem
    //
    //  Applies a brief, alternating horizontal impulse to every dynamic body
    //  once every kWindPeriod seconds.  Demonstrates:
    //    - Subclassing ecs::ISystem
    //    - Time accumulation inside a system Update hook
    //    - world.ForEach<T> on RigidBodyComponent
    // =========================================================================
    namespace
    {
        class WindGustSystem : public ecs::ISystem
        {
        public:
            void Update(ecs::World& world, float dt) override
            {
                m_elapsed += dt;
                if (m_elapsed < kWindPeriod)
                    return;

                m_elapsed  = 0.0f;
                m_gustDir  = -m_gustDir;                           // alternate direction
                const float impulse = m_gustDir * kWindImpulse;

                world.ForEach<physics::RigidBodyComponent>(
                    [&](ecs::EntityId, physics::RigidBodyComponent& rb)
                    {
                        if (rb.invMass > 0.0f)
                            rb.vx += impulse;
                    });
            }

        private:
            float m_elapsed  {0.0f};
            float m_gustDir  {1.0f};

            static constexpr float kWindPeriod {6.0f};   // seconds between gusts
            static constexpr float kWindImpulse{3.0f};   // m/s applied per gust
        };
    } // anonymous namespace


    // =========================================================================
    //  FullDemoScenario
    // =========================================================================
    class FullDemoScenario : public IScenario
    {
    public:

        // ---------------------------------------------------------------------
        //  Setup
        // ---------------------------------------------------------------------
        void Setup(ecs::World& world) override
        {
            m_renderer = std::make_unique<ascii::TextRenderer>(kW, kH);

            // ---- Logger (instance-based) ------------------------------------
            core::Logger log;
            log.Info("FullDemoScenario: initialising");

            // ---- Physics system + JobSystem + PhysicsSettings ---------------
            physics::EnvironmentForces env;
            env.gravityY = -9.81f;
            env.drag     =  0.02f;    // air resistance — damps pendulum to rest

            auto phys = std::make_unique<physics::PhysicsSystem>();
            physics::PhysicsSettings cfg;
            cfg.substeps             = 16;  // high stability
            cfg.constraintIterations = 16;  // stable rigid chain
            cfg.positionIterations   = 20;
            phys->SetSettings(cfg);
            phys->SetEnvironment(env);
            phys->SetJobSystem(&m_jobs);    // pass owned JobSystem
            world.AddSystem(std::move(phys));

            // ---- Custom system (added after physics so it runs each step) ---
            world.AddSystem(std::make_unique<WindGustSystem>());

            // ---- Static arena walls (thick to prevent tunnelling) -----------
            //  Inner bounds: X ∈ [kLeftX, kRightX], Y ∈ [kFloorY, kArenaTop]
            MakeWall(world,               0.0f, kFloorY   - 50.0f,  80.0f, 100.0f); // floor
            MakeWall(world, kLeftX  - 50.0f,  0.0f,  100.0f,  40.0f); // left
            MakeWall(world, kRightX + 50.0f,  0.0f,  100.0f,  40.0f); // right

            // ---- Tower platform (right of centre, surface Y = 0) -----------
            //  Raised shelf for the box tower so the pendulum can reach it.
            MakeWall(world, 2.0f, -1.0f, 8.0f, 2.0f);

            // ---- Pendulum chain + heavy wrecking ball -----------------------
            //  Anchor at (kAnchorX, kAnchorY) — static, attached to ceiling.
            //  kChainCount links placed at 45° SW so the chain falls clockwise
            //  (rightward) into the box tower on the right side.
            //  Each inter-link step is (−1.41, −1.41): dist = √(1.41²+1.41²) ≈ 2.0
            //  which matches kLinkDist, so the chain starts nearly taut.
            auto anchor = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(anchor, kAnchorX, kAnchorY, 0.0f);
            {
                auto& ab = world.AddComponent<physics::RigidBodyComponent>(anchor);
                ab.mass = 0.0f; ab.invMass = 0.0f;
            }
            m_anchorId = anchor;

            ecs::EntityId prev = anchor;
            for (int i = 0; i < kChainCount; ++i)
            {
                const float lx = kAnchorX - static_cast<float>(i + 1) * 1.41f;
                const float ly = kAnchorY - static_cast<float>(i + 1) * 1.41f;

                auto link = world.CreateEntity();
                world.AddComponent<physics::TransformComponent>(link, lx, ly, 0.0f);
                auto& lb = world.AddComponent<physics::RigidBodyComponent>(link);

                const bool isBall = (i == kChainCount - 1);
                if (isBall)
                {
                    // Heavy wrecking ball — large mass, circle collider
                    lb.mass        = 25.0f;
                    lb.invMass     = 1.0f / 25.0f;
                    lb.restitution = 0.4f;
                    lb.friction    = 0.3f;
                    lb.angularDrag = 0.15f;
                    world.AddComponent<physics::CircleColliderComponent>(link, kBallR);
                    physics::ConfigureCircleInertia(lb, kBallR);
                    m_ballId = link;
                }
                else
                {
                    // Small chain link
                    lb.mass        = 0.6f;
                    lb.invMass     = 1.0f / 0.6f;
                    lb.restitution = 0.1f;
                    world.AddComponent<physics::CircleColliderComponent>(link, 0.22f);
                    physics::ConfigureCircleInertia(lb, 0.22f);
                }

                // Distance joint connecting this link to the previous one
                auto& jc         = world.AddComponent<physics::DistanceJointComponent>(link);
                jc.entityA       = prev;
                jc.entityB       = link;
                jc.targetDistance = kLinkDist;
                jc.compliance    = 0.0f;  // fully rigid

                m_chainIds.push_back(link);
                prev = link;
            }

            // ---- Tower of dynamic AABB boxes (on raised platform) -----------
            //  kTowerCols columns × kTowerRows rows, sitting on the tower platform.
            log.Info("FullDemoScenario: building box tower");
            for (int row = 0; row < kTowerRows; ++row)
            {
                for (int col = 0; col < kTowerCols; ++col)
                {
                    const float bx = kTowerLeft  + (static_cast<float>(col) + 0.5f) * kBoxSize;
                    const float by = kTowerBaseY + (static_cast<float>(row) + 0.5f) * kBoxSize;

                    auto box = world.CreateEntity();
                    world.AddComponent<physics::TransformComponent>(box, bx, by, 0.0f);
                    auto& bb = world.AddComponent<physics::RigidBodyComponent>(box);
                    bb.mass        = 1.5f;
                    bb.invMass     = 1.0f / 1.5f;
                    bb.friction    = 0.7f;
                    bb.restitution = 0.1f;
                    bb.angularDrag = 0.15f;
                    world.AddComponent<physics::AABBComponent>(
                        box,
                        bx - kBoxSize * 0.5f, by - kBoxSize * 0.5f,
                        bx + kBoxSize * 0.5f, by + kBoxSize * 0.5f);
                    physics::ConfigureBoxInertia(bb, kBoxSize, kBoxSize);
                    m_boxIds.push_back(box);
                }
            }

            // ---- Bouncing circle particles (left half of arena) ------------
            //  Randomised starting positions and upward velocities.
            //  Demonstrates high-entity-count circle collisions.
            log.Info("FullDemoScenario: spawning particles");
            std::mt19937 rng{2025u};
            std::uniform_real_distribution<float> rndX{kLeftX + 1.0f, -2.0f};
            std::uniform_real_distribution<float> rndY{kFloorY + 0.5f,  3.0f};
            std::uniform_real_distribution<float> rndVx{-4.0f,  4.0f};
            std::uniform_real_distribution<float> rndVy{ 2.0f, 10.0f};

            for (int i = 0; i < kParticles; ++i)
            {
                const float px = rndX(rng), py = rndY(rng);
                auto p = world.CreateEntity();
                world.AddComponent<physics::TransformComponent>(p, px, py, 0.0f);
                auto& pb = world.AddComponent<physics::RigidBodyComponent>(p);
                pb.mass        = 0.12f;
                pb.invMass     = 1.0f / 0.12f;
                pb.restitution = 0.80f;
                pb.friction    = 0.05f;
                pb.vx          = rndVx(rng);
                pb.vy          = rndVy(rng);
                world.AddComponent<physics::CircleColliderComponent>(p, 0.30f);
                physics::ConfigureCircleInertia(pb, 0.30f);
                m_partIds.push_back(p);
            }

            // ---- GetStorage demo — log total RigidBody count after setup ----
            if (auto* rbs = world.GetStorage<physics::RigidBodyComponent>())
            {
                log.Info("FullDemoScenario: total RigidBody entities = "
                         + std::to_string(rbs->Size()));
            }
        }

        // ---------------------------------------------------------------------
        //  Update — scenario logic hook only.
        //  The engine owns world.Update(); calling it here would violate the
        //  IScenario contract tested by scenario_update_contract_tests.
        // ---------------------------------------------------------------------
        void Update(ecs::World& /*world*/, float /*dt*/) override {}

        // ---------------------------------------------------------------------
        //  Render — full ASCII showcase using every drawing primitive and all
        //  eight Color values.
        // ---------------------------------------------------------------------
        void Render(ecs::World& world, std::ostream& out) override
        {
            m_renderer->Clear();

            // ---- Arena border: DrawRect (White '+') -------------------------
            //  Maps inner bounds [kLeftX..kRightX] × [kFloorY..kArenaTop]
            //  to screen rectangle.
            const int bx0 = toSX(kLeftX),    by0 = toSY(kArenaTop);
            const int bx1 = toSX(kRightX),   by1 = toSY(kFloorY);
            m_renderer->DrawRect(bx0, by0, bx1 - bx0, by1 - by0,
                                 '+', ascii::Color::White);

            // ---- Tower platform: DrawLine (Yellow '=') ---------------------
            m_renderer->DrawLine(toSX(-2.0f), toSY(0.0f),
                                 toSX( 6.0f), toSY(0.0f),
                                 '=', ascii::Color::Yellow);

            // ---- Tower boxes: View<TransformComponent, AABBComponent> ------
            //  Uses multi-component View to select only entities that have
            //  both a transform and an AABB.  Static bodies (invMass==0) are
            //  skipped so only the dynamic tower blocks are drawn.
            world.View<physics::TransformComponent, physics::AABBComponent>(
                [&](ecs::EntityId id,
                    physics::TransformComponent& t,
                    physics::AABBComponent& /*aabb*/)
                {
                    auto* rb = world.GetComponent<physics::RigidBodyComponent>(id);
                    if (!rb || rb->invMass == 0.0f) return;
                    const int sx = toSX(t.x), sy = toSY(t.y);
                    if (inBounds(sx, sy))
                        m_renderer->Put(sx, sy, '#', ascii::Color::Cyan);
                });

            // ---- Particles: iterate tracked IDs (Green '.') ----------------
            for (ecs::EntityId pid : m_partIds)
            {
                if (auto* t = world.GetComponent<physics::TransformComponent>(pid))
                {
                    const int sx = toSX(t->x), sy = toSY(t->y);
                    if (inBounds(sx, sy))
                        m_renderer->Put(sx, sy, '.', ascii::Color::Green);
                }
            }

            // ---- Chain segments: DrawLine (Yellow '-') ---------------------
            //  Draws a line between each consecutive pair of link transforms
            //  starting from the anchor.
            {
                int px = toSX(kAnchorX), py = toSY(kAnchorY);
                for (ecs::EntityId cid : m_chainIds)
                {
                    if (auto* t = world.GetComponent<physics::TransformComponent>(cid))
                    {
                        const int nx = toSX(t->x), ny = toSY(t->y);
                        m_renderer->DrawLine(px, py, nx, ny, '-', ascii::Color::Yellow);
                        px = nx; py = ny;
                    }
                }
            }

            // ---- Wrecking ball: DrawEllipse outer ring + FillEllipse body --
            //  DrawEllipse (Red ':') draws the impact halo one cell larger than
            //  the filled body, giving the ball a distinct outline.
            if (auto* bt = world.GetComponent<physics::TransformComponent>(m_ballId))
            {
                const int bsx = toSX(bt->x), bsy = toSY(bt->y);
                const int rx  = static_cast<int>(kBallR * kSX);  // = 3
                const int ry  = static_cast<int>(kBallR * kSY);  // = 3
                m_renderer->DrawEllipse(bsx, bsy, rx + 1, ry + 1, ':', ascii::Color::Red);
                m_renderer->FillEllipse (bsx, bsy, rx,     ry,     'O', ascii::Color::Red);
            }

            // ---- Anchor: DrawCircle pivot ring (Blue) + Put 'X' (Magenta) --
            //  DrawCircle draws a small pivot indicator around the anchor point.
            const int asx = toSX(kAnchorX), asy = toSY(kAnchorY);
            m_renderer->DrawCircle(asx, asy, 2, 'o', ascii::Color::Blue);
            m_renderer->Put(asx, asy, 'X', ascii::Color::Magenta);

            // ---- Title bar (Magenta, row 0) ---------------------------------
            static constexpr const char* kTitle = " ATLASCORE FULL DEMO ";
            const int tx = (kW - static_cast<int>(std::strlen(kTitle))) / 2;
            for (int i = 0; kTitle[i] != '\0'; ++i)
                m_renderer->Put(tx + i, 0, kTitle[i], ascii::Color::Magenta);

            // ---- Feature legend (White, bottom two rows) -------------------
            static constexpr const char* kLegend[2] =
            {
                "ECS|JOBS|PHYSICS|JOINTS",
                "ASCII|CUSTOM-SYS|ALL-8-CLR",
            };
            for (int r = 0; r < 2; ++r)
            {
                const int lx = kW - static_cast<int>(std::strlen(kLegend[r])) - 1;
                const int ly = kH - 2 + r;
                for (int i = 0; kLegend[r][i] != '\0'; ++i)
                    m_renderer->Put(lx + i, ly, kLegend[r][i], ascii::Color::White);
            }

            m_renderer->SetHeadless(simlab::IsHeadlessRendering());
            m_renderer->PresentDiff(out);
        }

    private:

        // ---- World / screen constants ---------------------------------------
        static constexpr float kFloorY   = -9.5f;
        static constexpr float kLeftX    = -19.0f;
        static constexpr float kRightX   =  19.0f;
        static constexpr float kArenaTop =   9.5f;

        static constexpr int   kW  = 80;
        static constexpr int   kH  = 40;
        static constexpr float kSX = 2.0f;   // screen units per world unit (X)
        static constexpr float kSY = 2.0f;   // screen units per world unit (Y)

        // ---- Chain / ball ---------------------------------------------------
        static constexpr float kAnchorX    = -5.0f;
        static constexpr float kAnchorY    =  9.0f;
        static constexpr float kLinkDist   =  2.0f;
        static constexpr float kBallR      =  1.5f;
        static constexpr int   kChainCount =  4;    // 3 small links + 1 ball

        // ---- Tower ----------------------------------------------------------
        static constexpr float kBoxSize     =  1.4f;
        static constexpr float kTowerLeft   = -1.0f;
        static constexpr float kTowerBaseY  =  0.0f;   // top of tower platform
        static constexpr int   kTowerCols   =  4;
        static constexpr int   kTowerRows   =  3;

        // ---- Particles ------------------------------------------------------
        static constexpr int   kParticles  = 30;

        // ---- Coordinate mapping helpers ------------------------------------
        //  World X ∈ [−20, 20] → screen X ∈ [0, 80]
        //  World Y ∈ [−10, 10] → screen Y ∈ [40, 0]  (Y-axis flipped)
        int toSX(float wx) const
        {
            return static_cast<int>((wx + 20.0f) * kSX);
        }
        int toSY(float wy) const
        {
            return static_cast<int>((10.0f - wy) * kSY);
        }
        bool inBounds(int sx, int sy) const
        {
            return sx >= 0 && sx < kW && sy >= 0 && sy < kH;
        }

        // ---- Entity factory: static thick-walled AABB ----------------------
        static void MakeWall(ecs::World& world,
                              float cx, float cy, float w, float h)
        {
            auto e = world.CreateEntity();
            world.AddComponent<physics::TransformComponent>(e, cx, cy, 0.0f);
            auto& b = world.AddComponent<physics::RigidBodyComponent>(e);
            b.mass = 0.0f; b.invMass = 0.0f;
            world.AddComponent<physics::AABBComponent>(
                e,
                cx - w * 0.5f, cy - h * 0.5f,
                cx + w * 0.5f, cy + h * 0.5f);
        }

        // ---- Owned resources -----------------------------------------------
        std::unique_ptr<ascii::TextRenderer> m_renderer;
        jobs::JobSystem                       m_jobs;

        // ---- Entity handles ------------------------------------------------
        ecs::EntityId              m_anchorId{0};
        ecs::EntityId              m_ballId  {0};
        std::vector<ecs::EntityId> m_chainIds;
        std::vector<ecs::EntityId> m_boxIds;
        std::vector<ecs::EntityId> m_partIds;
    };

    // -------------------------------------------------------------------------
    //  Factory function — registered in ScenarioRegistry
    // -------------------------------------------------------------------------
    std::unique_ptr<IScenario> CreateFullDemoScenario()
    {
        return std::make_unique<FullDemoScenario>();
    }

} // namespace simlab
