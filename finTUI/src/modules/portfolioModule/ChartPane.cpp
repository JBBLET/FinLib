// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/modules/portfolioModule/ChartPane.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

ChartPane::ChartPane(std::shared_ptr<IPortfolioDataSource> ds,
                     std::function<void(std::string, bool)> onStatus)
    : dataSource_(std::move(ds)), onStatus_(std::move(onStatus)) {
    chart_.setTitle("Portfolio Value — Last 30 Days");
    component_ = ftxui::Renderer([this] { return buildContent_(); });
}

ftxui::Component ChartPane::component() { return component_; }

void ChartPane::setPortfolio(PortfolioSummary s) {
    summary_ = std::move(s);
    timeSeries_ = {};
    chart_.setData({});
}

void ChartPane::onActivated() {
    if (timeSeries_.values.empty() && !summary_.id.empty()) loadTimeSeries_();
}

void ChartPane::loadTimeSeries_() {
    try {
        const int64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        const int64_t startMs = nowMs - 30LL * 86'400'000LL;
        timeSeries_ = dataSource_->getTimeSeries(summary_.id, startMs, nowMs, 86'400'000LL);
        chart_.setData(timeSeries_.values);
    } catch (const std::exception& e) {
        if (onStatus_) onStatus_(std::string("Error loading chart: ") + e.what(), true);
    }
}

ftxui::Element ChartPane::buildContent_() const {
    return ftxui::vbox({
        chart_.render(),
        ftxui::separator(),
        ftxui::text(" r Reload") | ftxui::dim,
    });
}

}  // namespace finui
