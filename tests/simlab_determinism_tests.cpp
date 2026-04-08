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

#include "ecs/World.hpp"
#include "simlab/Scenario.hpp"
#include "simlab/WorldHasher.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace
{
    using ScenarioFactory = std::unique_ptr<simlab::IScenario>(*)();

    std::vector<std::uint64_t> RunScenarioHashes(ScenarioFactory factory, int steps, float dt)
    {
        ecs::World world;
        auto scenario = factory();
        assert(scenario != nullptr);
        scenario->Setup(world);

        simlab::WorldHasher hasher;
        std::vector<std::uint64_t> hashes;
        hashes.reserve(static_cast<std::size_t>(steps));

        for (int i = 0; i < steps; ++i)
        {
            scenario->Update(world, dt);
            world.Update(dt);
            hashes.push_back(hasher.HashWorld(world));
        }

        return hashes;
    }

    void VerifyScenarioDeterminism(const char* key, ScenarioFactory factory)
    {
        constexpr int kSteps = 180;
        constexpr float kDt = 1.0f / 60.0f;

        const auto runA = RunScenarioHashes(factory, kSteps, kDt);
        const auto runB = RunScenarioHashes(factory, kSteps, kDt);

        assert(runA.size() == runB.size());
        assert(!runA.empty());
        assert(runA == runB && "Scenario hash stream diverged across identical runs");

        std::cout << "[PASS] simlab determinism: " << key << '\n';
    }
}

int main()
{
    simlab::SetHeadlessRendering(true);
    VerifyScenarioDeterminism("gravity", simlab::CreatePlanetaryGravityScenario);
    VerifyScenarioDeterminism("wrecking", simlab::CreateWreckingBallScenario);
    VerifyScenarioDeterminism("fluid", simlab::CreateParticleFluidScenario);
    VerifyScenarioDeterminism("demo", simlab::CreateFullDemoScenario);
    std::cout << "Simlab determinism tests passed\n";
    return 0;
}
