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

#include <cassert>
#include <iostream>
#include <memory>

namespace
{
    class CountingSystem final : public ecs::ISystem
    {
    public:
        explicit CountingSystem(int* updates) : m_updates(updates) {}

        void Update(ecs::World& world, float dt) override
        {
            (void)world;
            (void)dt;
            ++(*m_updates);
        }

    private:
        int* m_updates;
    };

    void VerifyScenarioDoesNotStepWorld(const char* name,
                                        std::unique_ptr<simlab::IScenario> (*factory)())
    {
        ecs::World world;
        auto scenario = factory();
        scenario->Setup(world);

        int updates = 0;
        world.AddSystem(std::make_unique<CountingSystem>(&updates));

        const float dt = 1.0f / 60.0f;
        scenario->Update(world, dt);
        assert(updates == 0 && "Scenario::Update must not call world.Update");

        world.Update(dt);
        assert(updates == 1 && "Engine world.Update should step exactly once");

        (void)name;
    }
}

int main()
{
    VerifyScenarioDoesNotStepWorld("gravity", simlab::CreatePlanetaryGravityScenario);
    VerifyScenarioDoesNotStepWorld("wrecking", simlab::CreateWreckingBallScenario);
    VerifyScenarioDoesNotStepWorld("fluid", simlab::CreateParticleFluidScenario);

    std::cout << "Scenario update contract tests passed\n";
    return 0;
}
