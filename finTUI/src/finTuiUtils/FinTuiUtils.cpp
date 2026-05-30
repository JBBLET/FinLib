// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/finTuiUtils/FinTuiUtils.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace finui {

std::string Utils::fmtMoney(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
}

std::string Utils::fmtDate(int64_t ms) {
    auto tp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

std::string Utils::todayDateString() {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

int64_t Utils::parseDate(const std::string& s) {
    std::tm tm{};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
    time_t t = timegm(&tm);
    return static_cast<int64_t>(t) * 1000LL;
}

std::vector<std::string> Utils::asciiLineChart(const std::vector<double>& values, int w, int h) {
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

}  // namespace finui
