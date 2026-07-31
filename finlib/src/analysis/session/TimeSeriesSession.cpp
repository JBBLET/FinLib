// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finlib/analysis/session/TimeSeriesSession.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/Error.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Log.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace ts::analysis {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
TimeSeriesSession::TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                                     Timestamp startMs, Timestamp endMs, Timestamp frequencyMs)
    : service_{std::move(service)},
      seriesId_{std::move(seriesId)},
      startMs_{startMs},
      endMs_{endMs},
      frequencyMs_{frequencyMs} {
    source_ = std::make_shared<const TimeSeries>(service_->getRaw(seriesId_, startMs_, endMs_, frequencyMs));
    logging::info("loaded '{}' [{}..{}] size={}", seriesId_, startMs_, endMs_, source_->size());
}

TimeSeriesSession::TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                                     TimestampsPtr timestampsMs)
    : service_{std::move(service)}, seriesId_{std::move(seriesId)} {
    startMs_ = timestampsMs->front();
    endMs_ = timestampsMs->back();
    source_ = std::make_shared<const TimeSeries>(service_->getAligned(seriesId_, timestampsMs));
    logging::info("loaded '{}' [{}..{}] size={} (custom grid)", seriesId_, startMs_, endMs_, source_->size());
}

TimeSeriesSession::TimeSeriesSession(std::shared_ptr<const TimeSeries> precomputed)
    : service_{nullptr}, source_{std::move(precomputed)}, seriesId_{source_->getId()} {
    const auto ts = source_->getTimestamps();
    ensure<InvalidArgument>(!ts.empty(), "Cannot create TimeSeriesSession from empty TimeSeries");
    startMs_ = ts.front();
    endMs_ = ts.back();
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------
void TimeSeriesSession::setRange(Timestamp newStartMs, Timestamp newEndMs) {
    if (newStartMs == startMs_ && newEndMs == endMs_) return;
    logging::debug("setRange [{}..{}]", newStartMs, newEndMs);
    if (newStartMs < startMs_ || newEndMs > endMs_)
        extendRange_(std::min(newStartMs, startMs_), std::max(newEndMs, endMs_));
    startMs_ = newStartMs;
    endMs_ = newEndMs;
    invalidateAllCache_();
}

void TimeSeriesSession::setFrequency(Timestamp newFrequencyMs) {
    ensure(service_ != nullptr, "Cannot change frequency on a computed TimeSeriesSession");
    logging::debug("setFrequency {}ms", newFrequencyMs);
    frequencyMs_ = newFrequencyMs;
    source_ = std::make_shared<const TimeSeries>(service_->getRaw(seriesId_, startMs_, endMs_, newFrequencyMs));
    invalidateAllCache_();
}

void TimeSeriesSession::addTransform(std::string name, DerivedTransform transform) {
    logging::debug("addTransform '{}' (source → derived)", name);
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
    logging::debug("addTransform '{}' ({} inputs)", name, inputs.size());
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

    ensure(transforms_.find(name) != transforms_.end(), "No transform named '{}' registered on this session", name);
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
    ensure(transforms_.find(name) != transforms_.end(), "No transform named '{}' registered on this session", name);
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
    throw Exception("Session is not on a regular TimeSeries");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void TimeSeriesSession::extendRange_(Timestamp newStartMs, Timestamp newEndMs) {
    if (!service_) return;  // computed series — source is fixed, window only
    logging::debug("extendRange_ [{}..{}]", newStartMs, newEndMs);
    if (frequencyMs_.has_value()) {
        source_ =
            std::make_shared<const TimeSeries>(service_->getRaw(seriesId_, newStartMs, newEndMs, frequencyMs_.value()));
    } else {
        // Grid-built session with no fixed frequency: refetch native data, capping the bucket
        // resolution at the current source's spacing.
        const auto sp = source_->getTimestamps();
        Timestamp freq = (sp.size() >= 2) ? (sp[1] - sp[0]) : 86'400'000LL;
        if (freq <= 0) freq = 86'400'000LL;
        source_ = std::make_shared<const TimeSeries>(service_->getRaw(seriesId_, newStartMs, newEndMs, freq));
    }
}

CustomTimeSeriesAnalysis& TimeSeriesSession::customAnalysis(const std::string& name) {
    if (name.empty()) {
        if (!sourceCustomAnalysis_.has_value()) {
            logging::debug("customAnalysis: creating source custom analysis");
            sourceCustomAnalysis_ = CustomTimeSeriesAnalysis("", sourceView());
        }
        return sourceCustomAnalysis_.value();
    }
    auto& ca = derivedCustomAnalysisCache_[name];
    if (!ca.has_value()) {
        logging::debug("customAnalysis: creating '{}'", name);
        ca = CustomTimeSeriesAnalysis(name, derivedView(name));
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
    logging::debug("buildDerived_ '{}'", name);
    const SeriesNode& leaf = transforms_.at(name);
    std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> inputMap;
    inputMap.reserve(leaf.inputs.size());
    for (const auto& dep : leaf.inputs) {
        if (dep == "source" || dep.empty()) {
            // The source series is addressable both as the explicit "source" token and
            // as "" (the primary-series naming convention from ITimeSeriesSession), so a
            // caller can use one name for a series' input, view, and customAnalysis key.
            inputMap.emplace(dep, source_);
        } else {
            if (derivedCaches_.find(dep) == derivedCaches_.end()) {
                buildDerived_(dep);
            }
            inputMap.emplace(dep, derivedCaches_.at(dep));
        }
    }
    derivedCaches_[name] = std::make_shared<const TimeSeries>(leaf.transform(std::move(inputMap)));
}

}  // namespace ts::analysis
