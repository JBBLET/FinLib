// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "finapp/common/logger/ILogger.hpp"

namespace finapp::logging {

class ConsoleLogger : public ILogger {
 public:
    void write(Level lvl, const std::string& msg) override {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&t, &tm);
        std::ostringstream oss;
        oss << '[' << std::put_time(&tm, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count()
            << "][" << to_string(lvl) << "] " << msg << '\n';
        std::cout << oss.str() << std::flush;
    }
};

}  // namespace finapp::logging
