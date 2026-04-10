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

#include <cassert>
#include <iostream>
#include <string>

int main()
{
    const auto& all = simlab::ScenarioRegistry::All();
    assert(!all.empty());

    // Expect at least the new built-in scenarios
    bool hasGravity = false, hasFluid = false;
    for (const auto& d : all)
    {
        if (std::string(d.key) == "gravity") hasGravity = true;
        if (std::string(d.key) == "fluid") hasFluid = true;
    }
    assert(hasGravity);
    assert(hasFluid);

    // Create by key
    auto s1 = simlab::ScenarioRegistry::Create("gravity");
    auto s2 = simlab::ScenarioRegistry::Create("fluid");
    auto failSetup = simlab::ScenarioRegistry::Create("fail_setup");
    auto failUpdate = simlab::ScenarioRegistry::Create("fail_update");
    auto failWorldUpdate = simlab::ScenarioRegistry::Create("fail_world_update");
    auto failRender = simlab::ScenarioRegistry::Create("fail_render");
    assert(s1 != nullptr);
    assert(s2 != nullptr);
    assert(failSetup != nullptr);
    assert(failUpdate != nullptr);
    assert(failWorldUpdate != nullptr);
    assert(failRender != nullptr);

    const auto directSelection = simlab::ScenarioRegistry::ResolveScenarioSelection("fluid", 0);
    assert(directSelection.scenario != nullptr);
    assert(directSelection.requestedKey == "fluid");
    assert(directSelection.selectedKey == "fluid");
    assert(!directSelection.fallbackUsed);
    assert(!directSelection.shouldLogUnknownScenario);
    assert(!directSelection.shouldLogSelectedScenario);

    const auto fallbackSelection = simlab::ScenarioRegistry::ResolveScenarioSelection("does-not-exist", 0);
    assert(fallbackSelection.scenario != nullptr);
    assert(fallbackSelection.requestedKey == "does-not-exist");
    assert(fallbackSelection.selectedKey == "gravity");
    assert(fallbackSelection.fallbackUsed);
    assert(fallbackSelection.shouldLogUnknownScenario);
    assert(!fallbackSelection.shouldLogSelectedScenario);
    assert(fallbackSelection.unknownScenarioKey == "does-not-exist");

    const auto menuSelection = simlab::ScenarioRegistry::ResolveScenarioSelection("", 1);
    assert(menuSelection.scenario != nullptr);
    assert(menuSelection.requestedKey == "wrecking");
    assert(menuSelection.selectedKey == "wrecking");
    assert(!menuSelection.fallbackUsed);
    assert(!menuSelection.shouldLogUnknownScenario);
    assert(menuSelection.shouldLogSelectedScenario);

    const auto outOfRangeMenuSelection = simlab::ScenarioRegistry::ResolveScenarioSelection("", 99);
    assert(outOfRangeMenuSelection.scenario != nullptr);
    assert(outOfRangeMenuSelection.requestedKey == "gravity");
    assert(outOfRangeMenuSelection.selectedKey == "gravity");
    assert(!outOfRangeMenuSelection.fallbackUsed);
    assert(outOfRangeMenuSelection.shouldLogSelectedScenario);

    std::cout << "Scenario registry tests passed\n";
    return 0;
}
