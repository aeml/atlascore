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

#include <string>
#include <memory>
#include <iosfwd>

namespace core
{
    class Logger
    {
    public:
        Logger();

        void Info(const std::string& message) const;
        void Warn(const std::string& message) const;
        void Error(const std::string& message) const;

        // Configure a custom output stream (e.g., file). If not set,
        // logging falls back to std::cout.
        void SetOutput(std::shared_ptr<std::ostream> stream);

    private:
        std::shared_ptr<std::ostream> m_stream;
    };
}
