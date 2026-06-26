// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/analysis/ReturnFeatures.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/finance/analysis/FinanceMetrics.hpp"
#include "finapp/finance/analysis/ReturnTransforms.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace finance::analysis {

using ts::TimeSeries;
using ts::analysis::ITimeSeriesSession;

FeatureInstaller logReturnFeature() {
    return [](ITimeSeriesSession& session, const std::string& base) {
        FeatureBindings bindings;
        session.addTransform(
            "logReturn", {base}, [base](std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> aligned) {
                return logReturns(*aligned.at(base));
            });
        bindings.derivedSeries.push_back("logReturn");

        bindings.metrics.push_back(retainMetric(
            "logReturn",
            "sharpe",
            session.customAnalysis("logReturn").addMetric("logReturn", "sharpe", finapp::metrics::annualizedSharpe())));
        bindings.metrics.push_back(
            retainMetric("logReturn",
                         "annualizedVolatility",
                         session.customAnalysis("logReturn")
                             .addMetric("logReturn", "annualizedVolatility", finapp::metrics::annualizedVolatility())));
        return bindings;
    };
}

FeatureInstaller simpleReturnFeature() {
    return [](ITimeSeriesSession& session, const std::string& base) {
        FeatureBindings bindings;
        session.addTransform(
            "simpleReturn", {base}, [base](std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> aligned) {
                return simpleReturns(*aligned.at(base));
            });
        bindings.derivedSeries.push_back("simpleReturn");
        return bindings;
    };
}

FeatureInstaller totalReturnMetric() {
    return [](ITimeSeriesSession& session, const std::string& base) {
        FeatureBindings bindings;
        bindings.metrics.push_back(
            retainMetric(base,
                         "totalReturn",
                         session.customAnalysis(base).addMetric(base, "totalReturn", finapp::metrics::totalReturn())));
        return bindings;
    };
}

std::vector<FeatureInstaller> defaultReturnFeatures() {
    return {logReturnFeature(), simpleReturnFeature(), totalReturnMetric()};
}

}  // namespace finance::analysis
