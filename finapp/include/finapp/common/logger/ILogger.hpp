// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include <string>

namespace finapp::logging {

enum class Level { Debug, Info, Warning, Error };

inline const char* to_string(Level lvl) {
    switch (lvl) {
        case Level::Debug:   return "DEBUG";
        case Level::Info:    return "INFO";
        case Level::Warning: return "WARN";
        case Level::Error:   return "ERROR";
    }
    return "";
}

class ILogger {
 public:
    ILogger() = default;
    virtual ~ILogger() = default;
    ILogger(const ILogger&) = default;
    ILogger& operator=(const ILogger&) = default;
    ILogger(ILogger&&) = default;
    ILogger& operator=(ILogger&&) = default;

    virtual void write(Level lvl, const std::string& msg) = 0;
};

}  // namespace finapp::logging
