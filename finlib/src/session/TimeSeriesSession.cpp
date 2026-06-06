// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finlib/session/TimeSeriesSession.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace analysis {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
TimeSeriesSession::TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                                     TimestampMs startMs, TimestampMs endMs, TimestampMs frequencyMs)
    : service_{std::move(service)},
      seriesId_{std::move(seriesId)},
      startMs_{startMs},
      endMs_{endMs},
      frequencyMs_{frequencyMs} {
    source_ = std::make_shared<const TimeSeries>(service_->get(seriesId_, startMs_, endMs_, frequencyMs));
}

TimeSeriesSession::TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                                     std::shared_ptr<std::vector<TimestampMs>> timestampsMs)
    : service_{std::move(service)}, seriesId_{std::move(seriesId)} {
    startMs_ = timestampsMs->front();
    endMs_ = timestampsMs->back();
    source_ = std::make_shared<const TimeSeries>(service_->get(seriesId_, timestampsMs));
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------
void TimeSeriesSession::setRange(TimestampMs newStartMs, TimestampMs newEndMs) {
    if (newStartMs == startMs_ && newEndMs == endMs_) return;
    if (newStartMs < startMs_ || newEndMs > endMs_)
        extendRange_(std::min(newStartMs, startMs_), std::max(newEndMs, endMs_));
    startMs_ = newStartMs;
    endMs_ = newEndMs;
    invalidateAllCache_();
}

void TimeSeriesSession::setFrequency(TimestampMs newFrequencyMs) {
    frequencyMs_ = newFrequencyMs;
    source_ = std::make_shared<const TimeSeries>(service_->get(seriesId_, startMs_, endMs_, newFrequencyMs));
    invalidateAllCache_();
}

void TimeSeriesSession::addTransform(std::string name, DerivedTransform transform) {
    transforms_[name] = std::move(transform);
    derivedCaches_.erase(name);
    derivedAnalysisCache_.erase(name);
}

// ---------------------------------------------------------------------------
// View accessors
// ---------------------------------------------------------------------------
TimeSeriesView TimeSeriesSession::sourceView() const {
    size_t begin = source_->lowerBound(startMs_);
    size_t end = source_->upperBound(endMs_);
    return source_->slice(begin, end - begin);
}

TimeSeriesView TimeSeriesSession::derivedView(const std::string& name) const {
    if (transforms_.find(name) == transforms_.end())
        throw std::logic_error("No transform named '" + name + "' registered on this session");
    if (derivedCaches_.find(name) == derivedCaches_.end()) buildDerived_(name);
    const auto& derived = derivedCaches_.at(name);
    size_t begin = derived->lowerBound(startMs_);
    size_t end = derived->upperBound(endMs_);
    return derived->slice(begin, end - begin);
}

// ---------------------------------------------------------------------------
// Analysis accessors
// ---------------------------------------------------------------------------
const TimeSeriesAnalysis& TimeSeriesSession::sourceAnalysis() {
    if (!sourceAnalysis_.has_value()) sourceAnalysis_ = TimeSeriesAnalysis(sourceView());
    return sourceAnalysis_.value();
}

const TimeSeriesAnalysis& TimeSeriesSession::derivedAnalysis(const std::string& name) {
    auto& cached = derivedAnalysisCache_[name];
    if (!cached.has_value()) cached = TimeSeriesAnalysis(derivedView(name));
    return cached.value();
}

// ---------------------------------------------------------------------------
// Scalar accessors
// ---------------------------------------------------------------------------
size_t TimeSeriesSession::size() const { return source_->upperBound(endMs_) - source_->lowerBound(startMs_); }

TimestampMs TimeSeriesSession::frequencyMs() const {
    if (frequencyMs_.has_value()) return frequencyMs_.value();
    throw std::logic_error("Session is not on a regular TimeSeries");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void TimeSeriesSession::extendRange_(TimestampMs newStartMs, TimestampMs newEndMs) {
    if (frequencyMs_.has_value()) {
        source_ =
            std::make_shared<const TimeSeries>(service_->get(seriesId_, newStartMs, newEndMs, frequencyMs_.value()));
    } else {
        source_ = std::make_shared<const TimeSeries>(service_->getRaw(seriesId_, newStartMs, newEndMs));
    }
}

void TimeSeriesSession::invalidateAllCache_() {
    derivedCaches_.clear();
    sourceAnalysis_.reset();
    derivedAnalysisCache_.clear();
}

void TimeSeriesSession::buildDerived_(const std::string& name) const {
    derivedCaches_[name] = std::make_shared<const TimeSeries>(transforms_.at(name)(*source_));
}

}  // namespace analysis
