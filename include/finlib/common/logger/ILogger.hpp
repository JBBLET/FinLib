// Copyright 2026 JBBLET
#pragma once
#include <string>

namespace logging {

enum class Level { Debug, Info, Warning, Error };

const char* to_string(Level lvl);

class ILogger {
 public:
    virtual ~ILogger() = default;
    virtual void write(Level lvl, const std::string& msg) = 0;
};

}  // namespace logging
