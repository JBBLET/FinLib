// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/analysis/IAssetAnalysis.hpp"

#include <string>
#include <utility>
#include <vector>

namespace finance::analysis {

void IAssetAnalysis::installFeature(const FeatureInstaller& installer) {
    FeatureBindings bindings = installer(*session_, base_);
    derivedSeries_.insert(derivedSeries_.end(), bindings.derivedSeries.begin(), bindings.derivedSeries.end());
    for (auto& m : bindings.metrics) metrics_.push_back(std::move(m));
}

void IAssetAnalysis::installFeatures(const std::vector<FeatureInstaller>& installers) {
    for (const auto& installer : installers) installFeature(installer);
}

std::vector<std::pair<std::string, double>> IAssetAnalysis::scalarMetrics() {
    return computeMetricsOfType<double>(*session_, metrics_);
}

}  // namespace finance::analysis
