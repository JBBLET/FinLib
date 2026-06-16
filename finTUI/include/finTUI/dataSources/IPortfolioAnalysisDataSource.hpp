// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <string>

#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"

namespace finui {

class IPortfolioAnalysisDataSource {
 public:
    virtual ~IPortfolioAnalysisDataSource() = default;

    virtual PortfolioAnalysisData computeAnalysis(const std::string& portfolioId, int64_t startMs, int64_t endMs,
                                                  int64_t frequencyMs) = 0;
};

}  // namespace finui
