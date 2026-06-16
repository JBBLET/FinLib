// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/modules/portfolioModule/ChartPane.hpp"

#include <chrono>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

namespace {

constexpr int64_t kDayMs = 86'400'000LL;

// Returns Jan 1 00:00:00 UTC of the current year in milliseconds.
int64_t ytdStartMs() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    tm.tm_mon = 0;
    tm.tm_mday = 1;
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    return static_cast<int64_t>(timegm(&tm)) * 1000LL;
}

std::vector<TuiRangeSelector::Range> defaultRanges() {
    return {
        {"1D",  1 * kDayMs,          3'600'000LL},
        {"5D",  5 * kDayMs,          kDayMs},
        {"1M",  30 * kDayMs,         kDayMs},
        {"3M",  91 * kDayMs,         kDayMs},
        {"6M",  182 * kDayMs,        kDayMs},
        {"YTD", -1,                  kDayMs},   // sentinel: compute at load time
        {"1Y",  365 * kDayMs,        kDayMs},
        {"5Y",  5 * 365 * kDayMs,    7 * kDayMs},
        {"ALL", 0,                   kDayMs},   // sentinel: use portfolio earliest ts
    };
}

}  // namespace

ChartPane::ChartPane(std::shared_ptr<IPortfolioDataSource> ds, std::function<void(std::string, bool)> onStatus)
    : dataSource_(std::move(ds)), onStatus_(std::move(onStatus)) {
    // Default to 1M (index 2). CatchEvent routes events to the range selector when Chart tab is
    // active without adding it to the FTXUI focus tree, so event routing to other panes is unaffected.
    rangeSelector_ = ftxui::Make<TuiRangeSelector>(defaultRanges(), [this](const TuiRangeSelector::Range&) {
        if (!summary_.id.empty()) loadTimeSeries_();
    }, 2 /* 1M */);

    component_ = ftxui::CatchEvent(
        ftxui::Renderer([this] { return buildContent_(); }),
        [this](ftxui::Event e) -> bool { return rangeSelector_->OnEvent(e); });
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
        const auto& r = rangeSelector_->selected();

        int64_t startMs;
        if (r.durationMs == 0) {
            // ALL — use the earliest timestamp in the loaded series, or fallback to 10 years
            startMs = timeSeries_.timestamps.empty() ? nowMs - 10 * 365 * kDayMs : timeSeries_.timestamps.front();
            // Re-fetch with a very old start; server will clamp to available data
            startMs = nowMs - 10 * 365 * kDayMs;
        } else if (r.durationMs < 0) {
            // YTD sentinel
            startMs = ytdStartMs();
        } else {
            startMs = nowMs - r.durationMs;
        }

        timeSeries_ = dataSource_->getTimeSeries(summary_.id, startMs, nowMs, r.frequencyMs);
        chart_.setTitle("Portfolio Value — " + r.label);
        chart_.setData(timeSeries_.values);
    } catch (const std::exception& e) {
        if (onStatus_) onStatus_(std::string("Error loading chart: ") + e.what(), true);
    }
}

ftxui::Element ChartPane::buildContent_() const {
    return ftxui::vbox({
        rangeSelector_->Render(),
        ftxui::separator(),
        chart_.render(),
        ftxui::separator(),
        ftxui::text(" ←/→ Range  Enter Select  r Reload") | ftxui::dim,
    });
}

}  // namespace finui
