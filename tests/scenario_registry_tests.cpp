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
    assert(s1 != nullptr);
    assert(s2 != nullptr);

    std::cout << "Scenario registry tests passed\n";
    return 0;
}
