// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/finTuiUtils/TuiChartUtils.hpp"

#include <algorithm>

namespace finui::utils {

std::vector<std::string> TuiChartUtils::asciiLineChart(const std::vector<double>& values, int w, int h) {
    std::vector<std::string> grid(h, std::string(w, ' '));
    if (values.empty() || w <= 0 || h <= 0) return grid;

    double minVal = *std::min_element(values.begin(), values.end());
    double maxVal = *std::max_element(values.begin(), values.end());
    if (maxVal <= minVal) maxVal = minVal + 1.0;

    auto sample = [&](int col) -> double {
        if (static_cast<int>(values.size()) == 1) return values[0];
        size_t idx = static_cast<size_t>(static_cast<double>(values.size() - 1) * col / (w - 1));
        return values[idx];
    };

    auto toRow = [&](double v) -> int {
        int row = static_cast<int>((1.0 - (v - minVal) / (maxVal - minVal)) * (h - 1) + 0.5);
        return std::max(0, std::min(h - 1, row));
    };

    for (int x = 0; x < w; ++x) {
        const int row = toRow(sample(x));
        if (x == 0) {
            grid[row][x] = '*';
        } else {
            const int prev = toRow(sample(x - 1));
            if (row == prev) {
                grid[row][x] = '-';
            } else if (row < prev) {
                grid[row][x] = '/';
                for (int r = row + 1; r < prev; ++r) grid[r][x] = '|';
            } else {
                for (int r = prev + 1; r < row; ++r) grid[r][x] = '|';
                grid[row][x] = '\\';
            }
        }
    }
    return grid;
}

}  // namespace finui::utils
