// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include "finTUI/finTuiUtils/CurrencyUtils.hpp"
#include "finTUI/finTuiUtils/Utils.hpp"

namespace finui::utils {

// Aggregate for portfolio module code (requires portfolio_service_proto).
// Extends Utils with CurrencyUtils::currencyDisplay(double, finapp_rpc::Currency).
struct PortfolioUtils : Utils, CurrencyUtils {
    PortfolioUtils() = delete;
};

}  // namespace finui::utils
