// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/modules/portfolioModule/OverviewPane.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <utility>

#include "finTUI/finTuiUtils/PortfolioUtils.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace finui {

namespace {

constexpr int64_t kDayMs = 86'400'000LL;
constexpr int64_t kOneYearMs = 365 * kDayMs;

std::string fmtPct(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f%%", v * 100.0);
    return buf;
}

std::string fmtStat(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", v);
    return buf;
}

}  // namespace

OverviewPane::OverviewPane(std::shared_ptr<IPortfolioAnalysisDataSource> analysisSrc,
                           std::function<void(std::string, bool)> onStatus)
    : analysisSrc_(std::move(analysisSrc)), onStatus_(std::move(onStatus)) {
    component_ = ftxui::Renderer([this] { return buildContent_(); });
}

ftxui::Component OverviewPane::component() { return component_; }

void OverviewPane::setPortfolio(PortfolioSummary s) {
    summary_ = std::move(s);
    analysis_ = {};
    analysisLoaded_ = false;
}

void OverviewPane::onActivated() {
    if (!analysisLoaded_ && !summary_.id.empty()) loadAnalysis_();
}

void OverviewPane::loadAnalysis_() {
    try {
        const int64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        analysis_ = analysisSrc_->computeAnalysis(summary_.id, nowMs - kOneYearMs, nowMs, kDayMs);
        analysisLoaded_ = true;
    } catch (const std::exception& e) {
        if (onStatus_) onStatus_(std::string("Analysis error: ") + e.what(), true);
    }
}

ftxui::Element OverviewPane::buildSummarySection_() const {
    const auto& s = summary_;

    ftxui::Elements cashRows;
    for (const auto& c : s.cashBalances) {
        cashRows.push_back(ftxui::hbox({
            ftxui::text("  " + c.currency + ": ") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8),
            ftxui::text(utils::PortfolioUtils::fmtNumber(c.amount)),
        }));
    }
    if (cashRows.empty()) cashRows.push_back(ftxui::text("  —") | ftxui::dim);

    ftxui::Elements posRows;
    posRows.push_back(ftxui::hbox({
        ftxui::text(" Ticker") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
        ftxui::text("Quantity") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 14),
        ftxui::text("Value") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 14),
        ftxui::text("Weight") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 9),
    }));
    posRows.push_back(ftxui::separator());
    if (s.positions.empty()) {
        posRows.push_back(ftxui::text("  No positions") | ftxui::dim);
    } else {
        for (const auto& p : s.positions) {
            posRows.push_back(ftxui::hbox({
                ftxui::text(" " + p.ticker) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
                ftxui::text(utils::PortfolioUtils::fmtNumber(p.quantity)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 14),
                ftxui::text(utils::PortfolioUtils::fmtNumber(p.value)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 14),
                ftxui::text(utils::PortfolioUtils::percentDisplay(p.weight)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 9),
            }));
        }
    }

    return ftxui::vbox({
        ftxui::hbox({ftxui::text(" Total Value: ") | ftxui::bold,
                     ftxui::text(utils::PortfolioUtils::currencyStringDisplay(
                         s.totalValue, s.baseCurrency)) |
                         ftxui::color(ftxui::Color::Green)}),
        ftxui::separator(),
        ftxui::text(" Cash") | ftxui::bold,
        ftxui::vbox(std::move(cashRows)),
        ftxui::separator(),
        ftxui::text(" Positions") | ftxui::bold,
        ftxui::vbox(std::move(posRows)),
    });
}

ftxui::Element OverviewPane::buildNavStatsSection_() const {
    if (!analysisLoaded_) {
        return ftxui::vbox({
            ftxui::separator(),
            ftxui::text(" NAV Analysis (1Y)") | ftxui::bold,
            ftxui::text("  Loading…") | ftxui::dim,
        });
    }
    const auto& ns = analysis_.navStats;
    return ftxui::vbox({
        ftxui::separator(),
        ftxui::text(" NAV Analysis (1Y)") | ftxui::bold,
        ftxui::hbox({
            ftxui::text("  Mean: ") | ftxui::dim,
            ftxui::text(fmtPct(ns.mean)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
            ftxui::text("StdDev: ") | ftxui::dim,
            ftxui::text(fmtPct(ns.stdDev)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
        }),
        ftxui::hbox({
            ftxui::text("  Skew: ") | ftxui::dim,
            ftxui::text(fmtStat(ns.skewness)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
            ftxui::text("Kurt: ") | ftxui::dim,
            ftxui::text(fmtStat(ns.kurtosis)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
        }),
    });
}

ftxui::Element OverviewPane::buildPositionStatsSection_() const {
    if (!analysisLoaded_ || analysis_.positions.empty()) return ftxui::text("");

    bool hasReturnStats = false;
    for (const auto& pos : analysis_.positions) {
        if (pos.returnStats.has_value()) { hasReturnStats = true; break; }
    }
    if (!hasReturnStats) return ftxui::text("");

    ftxui::Elements rows;
    rows.push_back(ftxui::hbox({
        ftxui::text(" Ticker") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
        ftxui::text("Ret Mean") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
        ftxui::text("Ret StdDev") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
        ftxui::text("Skew") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
        ftxui::text("Kurt") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
    }));
    rows.push_back(ftxui::separator());

    for (const auto& pos : analysis_.positions) {
        if (!pos.returnStats.has_value()) continue;
        const auto& rs = *pos.returnStats;
        rows.push_back(ftxui::hbox({
            ftxui::text(" " + pos.ticker) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
            ftxui::text(fmtPct(rs.mean)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
            ftxui::text(fmtPct(rs.stdDev)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
            ftxui::text(fmtStat(rs.skewness)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
            ftxui::text(fmtStat(rs.kurtosis)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
        }));
    }

    return ftxui::vbox({
        ftxui::separator(),
        ftxui::text(" Position Returns (1Y)") | ftxui::bold,
        ftxui::vbox(std::move(rows)),
    });
}

ftxui::Element OverviewPane::buildContent_() const {
    if (summary_.id.empty()) {
        return ftxui::vbox(
            {ftxui::filler(), ftxui::text("  Select a portfolio and press Enter") | ftxui::dim, ftxui::filler()});
    }
    return ftxui::vbox({
        buildSummarySection_(),
        buildNavStatsSection_(),
        buildPositionStatsSection_(),
        ftxui::filler(),
        ftxui::separator(),
        ftxui::text(" t Add transaction  r Reload") | ftxui::dim,
    });
}

}  // namespace finui
