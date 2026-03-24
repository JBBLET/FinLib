// Copyright 2026 JBBLET
#pragma once
#include <iostream>
#include <string>

#include "finlib/common/logger/ILogger.hpp"

namespace logging {

class ConsoleLogger : public ILogger {
 public:
    void write(Level lvl, const std::string& msg) override {
        std::cout << "[" << to_string(lvl) << "] " << msg << '\n';
    }
};

}  // namespace logging
