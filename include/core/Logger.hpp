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
