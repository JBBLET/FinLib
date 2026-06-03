// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>
#include <vector>

namespace finui::utils {

struct TuiChartUtils {
    static std::vector<std::string> asciiLineChart(const std::vector<double>& values, int w, int h);
    TuiChartUtils() = delete;
};

}  // namespace finui::utils
