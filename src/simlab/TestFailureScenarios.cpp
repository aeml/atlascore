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

#include <stdexcept>
#include <memory>

namespace simlab
{
    namespace
    {
        class FailSetupScenario final : public IScenario
        {
        public:
            void Setup(ecs::World&) override
            {
                throw std::runtime_error("Test scenario setup failure");
            }

            void Update(ecs::World&, float) override {}
            void Render(ecs::World&, std::ostream&) override {}
        };

        class FailUpdateScenario final : public IScenario
        {
        public:
            void Setup(ecs::World&) override {}

            void Update(ecs::World&, float) override
            {
                throw std::runtime_error("Test scenario update failure");
            }

            void Render(ecs::World&, std::ostream&) override {}
        };

        class FailRenderScenario final : public IScenario
        {
        public:
            void Setup(ecs::World&) override {}
            void Update(ecs::World&, float) override {}

            void Render(ecs::World&, std::ostream&) override
            {
                throw std::runtime_error("Test scenario render failure");
            }
        };
    }

    std::unique_ptr<IScenario> CreateFailSetupScenario()
    {
        return std::make_unique<FailSetupScenario>();
    }

    std::unique_ptr<IScenario> CreateFailUpdateScenario()
    {
        return std::make_unique<FailUpdateScenario>();
    }

    std::unique_ptr<IScenario> CreateFailRenderScenario()
    {
        return std::make_unique<FailRenderScenario>();
    }
}
