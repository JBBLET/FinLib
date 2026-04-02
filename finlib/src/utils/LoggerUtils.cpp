// Copyright 2026 JBBLET
#include "finlib/common/logger/ILogger.hpp"

namespace logging {

inline const char* to_string(Level lvl) {
    switch (lvl) {
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Warning:
            return "WARN";
        case Level::Error:
            return "ERROR";
    }
    return "";
}
}  // namespace logging
