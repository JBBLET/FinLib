// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/uiComponents/TuiLineChart.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "finTUI/finTuiUtils/FinTuiUtils.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

TuiLineChart::TuiLineChart(int width, int height) : width_{width}, height_{height} {}

void TuiLineChart::setData(std::vector<double> values) { values_ = std::move(values); }
void TuiLineChart::setTitle(std::string title) { title_ = std::move(title); }

ftxui::Element TuiLineChart::render() const {
    if (values_.empty()) {
        return ftxui::vbox({ftxui::filler(), ftxui::text("  No data") | ftxui::dim, ftxui::filler()});
    }

    const auto lines = Utils::asciiLineChart(values_, width_, height_);
    const double minVal = *std::min_element(values_.begin(), values_.end());
    const double maxVal = *std::max_element(values_.begin(), values_.end());

    ftxui::Elements chartRows;
    for (int row = 0; row < height_; ++row) {
        std::string labelStr;
        if (row == 0 || row == height_ / 2 || row == height_ - 1) {
            const double v = maxVal - (maxVal - minVal) * row / (height_ - 1);
            labelStr = Utils::fmtMoney(v);
            if (static_cast<int>((labelStr.size()) > 10)) labelStr = labelStr.substr(0, 10);
        }
        const std::string padding(10 - static_cast<int>(labelStr.size()), ' ');
        chartRows.push_back(ftxui::hbox({
            ftxui::text(padding + labelStr) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
            ftxui::text("│"),
            ftxui::text(row < static_cast<int>(lines.size()) ? lines[row] : std::string(width_, ' ')),
        }));
    }
    chartRows.push_back(ftxui::hbox({
        ftxui::text(std::string(10, ' ')) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
        ftxui::text("└"),
        ftxui::text(std::string(width_, '-')),
    }));

    ftxui::Elements content;
    if (!title_.empty()) {
        content.push_back(ftxui::text(" " + title_) | ftxui::bold);
        content.push_back(ftxui::separator());
    }
    content.push_back(ftxui::vbox(std::move(chartRows)));
    content.push_back(ftxui::filler());

    return ftxui::vbox(std::move(content));
}

}  // namespace finui
