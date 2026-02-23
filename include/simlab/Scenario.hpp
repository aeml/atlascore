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
        // Scenario-specific logic hook. The engine owns world stepping.
        virtual void Update(ecs::World& world, float dt) = 0;
        virtual void Render(ecs::World& world, std::ostream& out) = 0;
    };

    std::unique_ptr<IScenario> CreatePlanetaryGravityScenario();
    std::unique_ptr<IScenario> CreateWreckingBallScenario();
    std::unique_ptr<IScenario> CreateParticleFluidScenario();
    std::unique_ptr<IScenario> CreateFullDemoScenario();

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
