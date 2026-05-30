// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>
#include <vector>

#include "ftxui/dom/elements.hpp"

namespace finui {

class TuiLineChart {
 public:
    explicit TuiLineChart(int width = 68, int height = 12);
    void setData(std::vector<double> values);
    void setTitle(std::string title);
    ftxui::Element render() const;

 private:
    int width_;
    int height_;
    std::string title_;
    std::vector<double> values_;
};

}  // namespace finui
