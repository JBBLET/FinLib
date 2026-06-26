// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <vector>

#include "finapp/finance/analysis/AnalysisFeature.hpp"

namespace finance::analysis {

// Derives "logReturn" from the base and registers sharpe + annualizedVolatility on it.
FeatureInstaller logReturnFeature();

// Derives "simpleReturn" from the base (no metrics by default).
FeatureInstaller simpleReturnFeature();

// Registers totalReturn on the base series itself (no derived series).
FeatureInstaller totalReturnMetric();

// Convenience bundle of the basic return features.
std::vector<FeatureInstaller> defaultReturnFeatures();

}  // namespace finance::analysis
