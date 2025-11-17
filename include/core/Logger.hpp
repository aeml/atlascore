#pragma once

#include <string>

namespace core
{
    class Logger
    {
    public:
        void Info(const std::string& message) const;
        void Warn(const std::string& message) const;
        void Error(const std::string& message) const;
    };
}
