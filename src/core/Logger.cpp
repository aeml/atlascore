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
