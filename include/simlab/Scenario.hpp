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
        virtual void Update(ecs::World& world, float dt) = 0;
        virtual void Render(ecs::World& world) = 0;
    };

    std::unique_ptr<IScenario> CreateDeterminismSmokeTest();
    std::unique_ptr<IScenario> CreateDeterminismHashScenario();
    std::unique_ptr<IScenario> CreateTextRendererPatternsScenario();
    std::unique_ptr<IScenario> CreateStackingScenario();
    std::unique_ptr<IScenario> CreatePendulumScenario();
    std::unique_ptr<IScenario> CreateClothScenario();
    std::unique_ptr<IScenario> CreateBallShowcaseScenario();
    std::unique_ptr<IScenario> CreateBallShowcaseScenario();

    using ScenarioFactory = std::unique_ptr<IScenario>(*)();

    struct ScenarioDesc
    {
        const char*    key;
        const char*    title;
        ScenarioFactory factory;
        const char*    category;    // optional grouping, e.g., "Text Tests"
        const char*    subcategory; // optional subgroup, e.g., "Text Renderer Tests"
    };

    class ScenarioRegistry
    {
    public:
        static void Register(const char* key, const char* title, ScenarioFactory factory);
        static void Register(const char* key, const char* title, ScenarioFactory factory,
                     const char* category, const char* subcategory);
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
        ScenarioAutoRegister(const char* key, const char* title, ScenarioFactory factory,
                             const char* category, const char* subcategory)
        {
            ScenarioRegistry::Register(key, title, factory, category, subcategory);
        }
    };

    void SetHeadlessRendering(bool enabled);
    bool IsHeadlessRendering();
}
