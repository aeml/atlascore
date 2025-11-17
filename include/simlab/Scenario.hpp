#pragma once

#include <memory>
#include <vector>
#include <string>

namespace ecs { class World; }

namespace simlab
{
    class IScenario
    {
    public:
        virtual ~IScenario() = default;
        virtual void Setup(ecs::World& world) = 0;
        virtual void Step(ecs::World& world, float dt) = 0;
    };

    std::unique_ptr<IScenario> CreateDeterminismSmokeTest();
    std::unique_ptr<IScenario> CreateDeterminismHashScenario();

    using ScenarioFactory = std::unique_ptr<IScenario>(*)();

    struct ScenarioDesc
    {
        const char*    key;
        const char*    title;
        ScenarioFactory factory;
    };

    class ScenarioRegistry
    {
    public:
        static void Register(const char* key, const char* title, ScenarioFactory factory);
        static const std::vector<ScenarioDesc>& All();
        static ScenarioFactory FindFactory(const std::string& key);
        static std::unique_ptr<IScenario> Create(const std::string& key);

    private:
        static std::vector<ScenarioDesc>& Storage();
    };

    struct ScenarioAutoRegister
    {
        ScenarioAutoRegister(const char* key, const char* title, ScenarioFactory factory)
        {
            ScenarioRegistry::Register(key, title, factory);
        }
    };
}
