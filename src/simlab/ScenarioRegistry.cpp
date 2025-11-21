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

#include <algorithm>
#include <atomic>
#include <mutex>

namespace simlab
{
    namespace
    {
        // Ensure thread-safe registration in case of static init across TUs
        std::mutex& regMutex()
        {
            static std::mutex m;
            return m;
        }
    }

    namespace
    {
        std::atomic<bool>& HeadlessFlag()
        {
            static std::atomic<bool> flag{false};
            return flag;
        }

        void RegisterBuiltIns()
        {
            // Ensure the two default scenarios are present without relying on TU static init
            ScenarioRegistry::Register("smoke", "ECS falling bodies smoke test", &CreateDeterminismSmokeTest);
            ScenarioRegistry::Register("hash",  "Determinism hash dual-run scenario", &CreateDeterminismHashScenario);
            ScenarioRegistry::Register("stack", "Pyramid stacking test", &CreateStackingScenario);
            ScenarioRegistry::Register("pendulum", "Pendulum chain test", &CreatePendulumScenario);
            ScenarioRegistry::Register("cloth", "Cloth simulation test", &CreateClothScenario);
            ScenarioRegistry::Register("ball_showcase", "Ball collision showcase", &CreateBallShowcaseScenario,
                                       "Physics Demos", "Rigid Bodies");
            ScenarioRegistry::Register("stress", "Stress Test (2000 particles)", &CreateStressTestScenario,
                                       "Benchmarks", "Physics");
            // Text renderer patterns (grouped under Text Tests / Text Renderer Tests)
            ScenarioRegistry::Register("text_patterns", "Text renderer patterns", &CreateTextRendererPatternsScenario,
                                       "Text Tests", "Text Renderer Tests");
        }
    }

    std::vector<ScenarioDesc>& ScenarioRegistry::Storage()
    {
        static std::vector<ScenarioDesc> s_storage;
        return s_storage;
    }

    void ScenarioRegistry::Register(const char* key, const char* title, ScenarioFactory factory)
    {
        if (!key || !title || !factory)
        {
            return;
        }
        std::lock_guard<std::mutex> lock{regMutex()};
        auto& st = Storage();
        auto it = std::find_if(st.begin(), st.end(), [&](const ScenarioDesc& d){ return std::string(d.key) == key; });
        if (it == st.end())
        {
            st.push_back(ScenarioDesc{key, title, factory, nullptr, nullptr});
        }
    }

    void ScenarioRegistry::Register(const char* key, const char* title, ScenarioFactory factory,
                                    const char* category, const char* subcategory)
    {
        if (!key || !title || !factory)
        {
            return;
        }
        std::lock_guard<std::mutex> lock{regMutex()};
        auto& st = Storage();
        auto it = std::find_if(st.begin(), st.end(), [&](const ScenarioDesc& d){ return std::string(d.key) == key; });
        if (it == st.end())
        {
            st.push_back(ScenarioDesc{key, title, factory, category, subcategory});
        }
        else
        {
            it->category = category;
            it->subcategory = subcategory;
        }
    }

    const std::vector<ScenarioDesc>& ScenarioRegistry::All()
    {
        static std::once_flag once;
        std::call_once(once, RegisterBuiltIns);
        return Storage();
    }

    ScenarioFactory ScenarioRegistry::FindFactory(const std::string& key)
    {
        static std::once_flag once;
        std::call_once(once, RegisterBuiltIns);
        auto& st = Storage();
        auto it = std::find_if(st.begin(), st.end(), [&](const ScenarioDesc& d){ return key == d.key; });
        if (it != st.end())
        {
            return it->factory;
        }
        return nullptr;
    }

    std::unique_ptr<IScenario> ScenarioRegistry::Create(const std::string& key)
    {
        static std::once_flag once;
        std::call_once(once, RegisterBuiltIns);
        auto f = FindFactory(key);
        if (f)
        {
            return f();
        }
        return {};
    }

    void SetHeadlessRendering(bool enabled)
    {
        HeadlessFlag().store(enabled, std::memory_order_relaxed);
    }

    bool IsHeadlessRendering()
    {
        return HeadlessFlag().load(std::memory_order_relaxed);
    }
}
