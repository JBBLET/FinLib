// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace finui {

struct Utils {
    static std::string fmtMoney(double v);
    static std::string fmtDate(int64_t ms);
    static std::string todayDateString();
    static int64_t parseDate(const std::string& s);
    static std::vector<std::string> asciiLineChart(const std::vector<double>& values, int w, int h);

    Utils() = delete;
};

}  // namespace finui
