// Copyright 2026 JBBLET
#pragma once

#include <cmath>
#include <format>
#include <string>
#include <string_view>

#include "Eigen/Dense"
#include "finlib/analysis/models/interfaces/IModel.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::models {

struct PredictionDistribution {
    double mean;
    double variance;
};

class IProbabilisticModel : public virtual IModel {
 public:
    virtual void setData(const TimeSeriesView& view, double trainRatio = 0.7, double validationRatio = 0.15) = 0;
    virtual PredictionDistribution predictDistribution(const Eigen::VectorXd& window) const = 0;
};

}  // namespace ts::models

// The standard deviation is derived rather than stored, because a forecast is read in the
// units of the series and the variance is in their square.
template <>
struct std::formatter<ts::models::PredictionDistribution> : std::formatter<std::string_view> {
    auto format(const ts::models::PredictionDistribution& distribution, std::format_context& ctx) const
        -> std::format_context::iterator {
        const std::string rendered = std::format("N(mean={}, var={}, sd={})",
                                                 ts::fmt::formatDouble(distribution.mean),
                                                 ts::fmt::formatDouble(distribution.variance),
                                                 ts::fmt::formatDouble(std::sqrt(distribution.variance)));
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};
