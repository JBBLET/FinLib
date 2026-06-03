// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include "finTUI/finTuiUtils/DateUtils.hpp"
#include "finTUI/finTuiUtils/FormatUtils.hpp"
#include "finTUI/finTuiUtils/TuiChartUtils.hpp"

namespace finui::utils {

// Aggregate for UI component code.
struct Utils : DateUtils, FormatUtils, TuiChartUtils {
    Utils() = delete;
};

}  // namespace finui::utils
