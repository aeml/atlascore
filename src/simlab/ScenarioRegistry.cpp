#include "simlab/Scenario.hpp"

#include <algorithm>
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
        void RegisterBuiltIns()
        {
            // Ensure the two default scenarios are present without relying on TU static init
            ScenarioRegistry::Register("smoke", "ECS falling bodies smoke test", &CreateDeterminismSmokeTest);
            ScenarioRegistry::Register("hash",  "Determinism hash dual-run scenario", &CreateDeterminismHashScenario);
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
}
