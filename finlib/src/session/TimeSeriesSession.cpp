// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finlib/session/TimeSeriesSession.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/logger/PrefixedLogger.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace analysis {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
TimeSeriesSession::TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                                     Timestamp startMs, Timestamp endMs, Timestamp frequencyMs,
                                     logging::ILogger* logger)
    : service_{std::move(service)},
      seriesId_{std::move(seriesId)},
      startMs_{startMs},
      endMs_{endMs},
      frequencyMs_{frequencyMs},
      logger_{logging::PrefixedLogger::wrap(logger, "TimeSeriesSession")} {
    source_ = std::make_shared<const TimeSeries>(service_->get(seriesId_, startMs_, endMs_, frequencyMs));
    if (logger_)
        logger_->write(logging::Level::Info,
                       "loaded '" + seriesId_ + "' [" + std::to_string(startMs_) + ".." + std::to_string(endMs_) +
                           "] size=" + std::to_string(source_->size()));
}

TimeSeriesSession::TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                                     TimestampsPtr timestampsMs, logging::ILogger* logger)
    : service_{std::move(service)},
      seriesId_{std::move(seriesId)},
      logger_{logging::PrefixedLogger::wrap(logger, "TimeSeriesSession")} {
    startMs_ = timestampsMs->front();
    endMs_ = timestampsMs->back();
    source_ = std::make_shared<const TimeSeries>(service_->get(seriesId_, timestampsMs));
    if (logger_)
        logger_->write(logging::Level::Info,
                       "loaded '" + seriesId_ + "' [" + std::to_string(startMs_) + ".." + std::to_string(endMs_) +
                           "] size=" + std::to_string(source_->size()) + " (custom grid)");
}

TimeSeriesSession::TimeSeriesSession(std::shared_ptr<const TimeSeries> precomputed)
    : service_{nullptr}, source_{std::move(precomputed)}, seriesId_{source_->getId()} {
    const auto ts = source_->getTimestamps();
    if (ts.empty()) throw std::invalid_argument("Cannot create TimeSeriesSession from empty TimeSeries");
    startMs_ = ts.front();
    endMs_ = ts.back();
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------
void TimeSeriesSession::setRange(Timestamp newStartMs, Timestamp newEndMs) {
    if (newStartMs == startMs_ && newEndMs == endMs_) return;
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "setRange [" + std::to_string(newStartMs) + ".." + std::to_string(newEndMs) + "]");
    if (newStartMs < startMs_ || newEndMs > endMs_)
        extendRange_(std::min(newStartMs, startMs_), std::max(newEndMs, endMs_));
    startMs_ = newStartMs;
    endMs_ = newEndMs;
    invalidateAllCache_();
}

void TimeSeriesSession::setFrequency(Timestamp newFrequencyMs) {
    if (!service_) throw std::logic_error("Cannot change frequency on a computed TimeSeriesSession");
    if (logger_) logger_->write(logging::Level::Debug, "setFrequency " + std::to_string(newFrequencyMs) + "ms");
    frequencyMs_ = newFrequencyMs;
    source_ = std::make_shared<const TimeSeries>(service_->get(seriesId_, startMs_, endMs_, newFrequencyMs));
    invalidateAllCache_();
}

void TimeSeriesSession::addTransform(std::string name, DerivedTransform transform) {
    if (logger_) logger_->write(logging::Level::Debug, "addTransform '" + name + "' (source → derived)");
    transforms_[name] =
        std::move(SeriesNode{name,        //
                             {"source"},  //
                             [transform](std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> map) {
                                 return transform(*map.at("source"));
                             }});
    derivedCaches_.erase(name);
    derivedAnalysisCache_.erase(name);
}

void TimeSeriesSession::addTransform(std::string name, std::vector<std::string> inputs, ComputeTransform transform) {
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "addTransform '" + name + "' (" + std::to_string(inputs.size()) + " inputs)");
    transforms_[name] = std::move(SeriesNode{name,               //
                                             std::move(inputs),  //
                                             std::move(transform)});
    derivedCaches_.erase(name);
    derivedAnalysisCache_.erase(name);
}
// ---------------------------------------------------------------------------
// ITimeSeriesSession
// ---------------------------------------------------------------------------
std::shared_ptr<const TimeSeries> TimeSeriesSession::seriesPtr(const std::string& name) {
    return name.empty() ? sourceTimeSeriesPtr() : derivedTimeSeriesPtr(name);
}

TimeSeriesView TimeSeriesSession::seriesView(const std::string& name) {
    return name.empty() ? sourceView() : derivedView(name);
}

const TimeSeriesAnalysis& TimeSeriesSession::seriesAnalysis(const std::string& name) {
    return name.empty() ? sourceAnalysis() : derivedAnalysis(name);
}

// ---------------------------------------------------------------------------
// TimeSeries accessors
// ---------------------------------------------------------------------------

std::shared_ptr<const TimeSeries> TimeSeriesSession::derivedTimeSeriesPtr(const std::string& name) {
    if (name == "") return sourceTimeSeriesPtr();

    if (transforms_.find(name) == transforms_.end())
        throw std::logic_error("No transform named '" + name + "' registered on this session");
    if (derivedCaches_.find(name) == derivedCaches_.end()) buildDerived_(name);
    return derivedCaches_.at(name);
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

Timestamp TimeSeriesSession::frequencyMs() const {
    if (frequencyMs_.has_value()) return frequencyMs_.value();
    throw std::logic_error("Session is not on a regular TimeSeries");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void TimeSeriesSession::extendRange_(Timestamp newStartMs, Timestamp newEndMs) {
    if (!service_) return;  // computed series — source is fixed, window only
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "extendRange_ [" + std::to_string(newStartMs) + ".." + std::to_string(newEndMs) + "]");
    if (frequencyMs_.has_value()) {
        source_ =
            std::make_shared<const TimeSeries>(service_->get(seriesId_, newStartMs, newEndMs, frequencyMs_.value()));
    } else {
        source_ = std::make_shared<const TimeSeries>(service_->getRaw(seriesId_, newStartMs, newEndMs));
    }
}

CustomTimeSeriesAnalysis& TimeSeriesSession::customAnalysis(const std::string& name) {
    if (name.empty()) {
        if (!sourceCustomAnalysis_.has_value()) {
            if (logger_) logger_->write(logging::Level::Debug, "customAnalysis: creating source custom analysis");
            sourceCustomAnalysis_ = CustomTimeSeriesAnalysis("", sourceView(), logger_.get());
        }
        return sourceCustomAnalysis_.value();
    }
    auto& ca = derivedCustomAnalysisCache_[name];
    if (!ca.has_value()) {
        if (logger_) logger_->write(logging::Level::Debug, "customAnalysis: creating '" + name + "'");
        ca = CustomTimeSeriesAnalysis(name, derivedView(name), logger_.get());
    }
    return ca.value();
}

void TimeSeriesSession::invalidateAllCache_() {
    derivedCaches_.clear();
    sourceAnalysis_.reset();
    derivedAnalysisCache_.clear();
    if (sourceCustomAnalysis_.has_value()) sourceCustomAnalysis_->rebind(sourceView());
    for (auto& [name, ca] : derivedCustomAnalysisCache_)
        if (ca.has_value()) ca->rebind(derivedView(name));
}

void TimeSeriesSession::buildDerived_(const std::string& name) const {
    if (logger_) logger_->write(logging::Level::Debug, "buildDerived_ '" + name + "'");
    const SeriesNode& leaf = transforms_.at(name);
    std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> inputMap;
    inputMap.reserve(leaf.inputs.size());
    for (const auto& dep : leaf.inputs) {
        if (dep == "source") {
            inputMap.emplace("source", source_);
        } else {
            if (derivedCaches_.find(dep) == derivedCaches_.end()) {
                buildDerived_(dep);
            }
            inputMap.emplace(dep, derivedCaches_.at(dep));
        }
    }
    derivedCaches_[name] = std::make_shared<const TimeSeries>(leaf.transform(std::move(inputMap)));
}

}  // namespace analysis
