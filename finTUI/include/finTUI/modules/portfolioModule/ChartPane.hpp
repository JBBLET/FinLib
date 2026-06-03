// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "finTUI/dataSources/IPortfolioDataSource.hpp"
#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"
#include "finTUI/uiComponents/TuiLineChart.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

class ChartPane {
 public:
    ChartPane(std::shared_ptr<IPortfolioDataSource> ds,
              std::function<void(std::string, bool)> onStatus);
    ChartPane(const ChartPane&) = delete;
    ChartPane(ChartPane&&) = delete;
    ChartPane& operator=(const ChartPane&) = delete;
    ChartPane& operator=(ChartPane&&) = delete;

    ftxui::Component component();
    void setPortfolio(PortfolioSummary s);
    void onActivated();

 private:
    std::shared_ptr<IPortfolioDataSource> dataSource_;
    std::function<void(std::string, bool)> onStatus_;

    PortfolioSummary summary_;
    TimeSeriesData timeSeries_;
    TuiLineChart chart_;

    ftxui::Component component_;

    void loadTimeSeries_();
    ftxui::Element buildContent_() const;
};

}  // namespace finui
