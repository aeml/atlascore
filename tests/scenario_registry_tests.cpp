#include "simlab/Scenario.hpp"

#include <cassert>
#include <iostream>

int main()
{
    const auto& all = simlab::ScenarioRegistry::All();
    assert(!all.empty());

    // Expect at least the two built-in scenarios
    bool hasSmoke = false, hasHash = false;
    for (const auto& d : all)
    {
        if (std::string(d.key) == "smoke") hasSmoke = true;
        if (std::string(d.key) == "hash") hasHash = true;
    }
    assert(hasSmoke);
    assert(hasHash);

    // Create by key
    auto s1 = simlab::ScenarioRegistry::Create("smoke");
    auto s2 = simlab::ScenarioRegistry::Create("hash");
    assert(s1 != nullptr);
    assert(s2 != nullptr);

    std::cout << "Scenario registry tests passed\n";
    return 0;
}
