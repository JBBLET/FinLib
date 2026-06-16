// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "finTUI/dataSources/IPortfolioAnalysisDataSource.hpp"
#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

class OverviewPane {
 public:
    OverviewPane(std::shared_ptr<IPortfolioAnalysisDataSource> analysisSrc,
                 std::function<void(std::string, bool)> onStatus);
    OverviewPane(const OverviewPane&) = delete;
    OverviewPane(OverviewPane&&) = delete;
    OverviewPane& operator=(const OverviewPane&) = delete;
    OverviewPane& operator=(OverviewPane&&) = delete;

    ftxui::Component component();
    void setPortfolio(PortfolioSummary s);
    void onActivated();

 private:
    std::shared_ptr<IPortfolioAnalysisDataSource> analysisSrc_;
    std::function<void(std::string, bool)> onStatus_;

    PortfolioSummary summary_;
    PortfolioAnalysisData analysis_;
    bool analysisLoaded_ = false;

    ftxui::Component component_;

    void loadAnalysis_();
    ftxui::Element buildContent_() const;
    ftxui::Element buildSummarySection_() const;
    ftxui::Element buildNavStatsSection_() const;
    ftxui::Element buildPositionStatsSection_() const;
};

}  // namespace finui
