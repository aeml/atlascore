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

#include "core/Logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace
{
    std::mutex g_logMutex;

    std::string CurrentTimeString()
    {
        using clock = std::chrono::system_clock;
        const auto now   = clock::now();
        const auto timeT = clock::to_time_t(now);

        std::tm localTm{};
    #if defined(_WIN32)
        localtime_s(&localTm, &timeT);
    #else
        localtime_r(&timeT, &localTm);
    #endif

        std::ostringstream oss;
        oss << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    void LogWithLevel(std::ostream& os, const char* level, const std::string& message)
    {
        std::lock_guard<std::mutex> lock{g_logMutex};
        os << "[" << CurrentTimeString() << "] "
           << level << ": " << message << '\n';
    }
}

namespace core
{
    Logger::Logger() = default;

    void Logger::Info(const std::string& message) const
    {
        auto& os = m_stream ? *m_stream : std::cout;
        LogWithLevel(os, "INFO", message);
    }

    void Logger::Warn(const std::string& message) const
    {
        auto& os = m_stream ? *m_stream : std::cout;
        LogWithLevel(os, "WARN", message);
    }

    void Logger::Error(const std::string& message) const
    {
        auto& os = m_stream ? *m_stream : std::cout;
        LogWithLevel(os, "ERROR", message);
    }

    void Logger::SetOutput(std::shared_ptr<std::ostream> stream)
    {
        m_stream = std::move(stream);
    }
}
