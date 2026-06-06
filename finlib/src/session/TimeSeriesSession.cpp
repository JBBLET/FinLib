// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finlib/session/TimeSeriesSession.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/logger/PrefixedLogger.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace analysis {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
TimeSeriesSession::TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                                     TimestampMs startMs, TimestampMs endMs, TimestampMs frequencyMs,
                                     logging::ILogger* logger)
    : service_{std::move(service)},
      seriesId_{seriesId},
      startMs_{startMs},
      endMs_{endMs},
      frequencyMs_{frequencyMs},
      logger_{logging::PrefixedLogger::wrap(logger, "TimeSeriesSession")} {
    source_ = std::make_shared<const TimeSeries>(service_->get(seriesId, startMs, endMs, frequencyMs));
    if (logger_)
        logger_->write(logging::Level::Info,
                       "loaded '" + seriesId_ + "' [" + std::to_string(startMs_) + ", " + std::to_string(endMs_) +
                           "] freq=" + std::to_string(frequencyMs) + "ms, " + std::to_string(source_->size()) + " pts");
}

TimeSeriesSession::TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                                     std::shared_ptr<std::vector<TimestampMs>> timestampsMs, logging::ILogger* logger)
    : service_{std::move(service)},
      seriesId_{seriesId},
      logger_{logging::PrefixedLogger::wrap(logger, "TimeSeriesSession")} {
    startMs_ = timestampsMs->front();
    endMs_ = timestampsMs->back();
    source_ = std::make_shared<const TimeSeries>(service_->get(seriesId_, timestampsMs));
    if (logger_)
        logger_->write(logging::Level::Info,
                       "loaded '" + seriesId_ + "' [" + std::to_string(startMs_) + ", " + std::to_string(endMs_) +
                           "] " + std::to_string(source_->size()) + " pts (custom timestamps)");
}

// ---------------------------------------------------------------------------
// Setters Methods
// ---------------------------------------------------------------------------
void TimeSeriesSession::setRange(TimestampMs newStartMs, TimestampMs newEndMs) {
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "setRange '" + seriesId_ + "' [" + std::to_string(startMs_) + ", " + std::to_string(endMs_) +
                           "] -> [" + std::to_string(newStartMs) + ", " + std::to_string(newEndMs) + "]");
    if (newStartMs < startMs_ && newEndMs > endMs_) {
        extendRange_(newStartMs, newEndMs);
    }
    if (newStartMs > startMs_ && newEndMs < endMs_) {
        reduceRange_(newStartMs, newEndMs);
    }
    if (newStartMs < startMs_ && newEndMs < endMs_) {
        extendRange_(newStartMs, endMs_);
        reduceRange_(newStartMs, newEndMs);
    }
    if (newStartMs > startMs_ && newEndMs > endMs_) {
        extendRange_(startMs_, newEndMs);
        reduceRange_(newStartMs, newEndMs);
    }
    startMs_ = newStartMs;
    endMs_ = newEndMs;
    invalidateAllCache_();
}

void TimeSeriesSession::setTransform(DerivedTransform transform) {
    if (logger_)
        logger_->write(logging::Level::Debug, "transform set for '" + seriesId_ + "'");
    transform_ = std::move(transform);
    derivedCache_ = nullptr;
    derivedAnalysis_.reset();
}

// ---------------------------------------------------------------------------
// View Accessors
// ---------------------------------------------------------------------------
TimeSeriesView TimeSeriesSession::sourceView() const {
    size_t begin = source_->lowerBound(startMs_);
    size_t end = source_->upperBound(endMs_);
    return source_->slice(begin, end - begin);
}

TimeSeriesView TimeSeriesSession::derivedView() const {
    if (!transform_) throw std::logic_error("No transform registered on this session");
    if (!derivedCache_) buildDerived_();

    size_t begin = derivedCache_->lowerBound(startMs_);
    size_t end = derivedCache_->upperBound(endMs_);  // exclusive
    return derivedCache_->slice(begin, end - begin);
}
// ---------------------------------------------------------------------------
// Analysis Accessors
// ---------------------------------------------------------------------------

const TimeSeriesAnalysis& TimeSeriesSession::sourceAnalysis() {
    if (sourceAnalysis_.has_value()) {
        return sourceAnalysis_.value();
    }
    if (logger_)
        logger_->write(logging::Level::Debug, "computing source analysis for '" + seriesId_ + "'");
    sourceAnalysis_ = TimeSeriesAnalysis(sourceView());
    return sourceAnalysis_.value();
}

const TimeSeriesAnalysis& TimeSeriesSession::derivedAnalysis() {
    if (derivedAnalysis_.has_value()) {
        return derivedAnalysis_.value();
    }
    if (logger_)
        logger_->write(logging::Level::Debug, "computing derived analysis for '" + seriesId_ + "'");
    derivedAnalysis_ = TimeSeriesAnalysis(derivedView());
    return derivedAnalysis_.value();
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
size_t TimeSeriesSession::size() const { return source_->upperBound(endMs_) - source_->lowerBound(startMs_); }

TimestampMs TimeSeriesSession::frequencyMs() const {
    if (frequencyMs_.has_value()) {
        return frequencyMs_.value();
    }
    throw std::logic_error("The Session is not on a Regular TimeSeries");
}
// ---------------------------------------------------------------------------
// private helpers
// ---------------------------------------------------------------------------
void TimeSeriesSession::extendRange_(TimestampMs newStartMs, TimestampMs newEndMs) {
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "extending source for '" + seriesId_ + "' to [" + std::to_string(newStartMs) + ", " +
                           std::to_string(newEndMs) + "]");
    if (frequencyMs_.has_value()) {
        source_ =
            std::make_shared<const TimeSeries>(service_->get(seriesId_, newStartMs, newEndMs, frequencyMs_.value()));
    } else {
        source_ = std::make_shared<const TimeSeries>(service_->getRaw(seriesId_, newStartMs, newEndMs));
    }
}
void TimeSeriesSession::reduceRange_(TimestampMs newStartMs, TimestampMs newEndMs) {
    startMs_ = newStartMs;
    endMs_ = newEndMs;
}
void TimeSeriesSession::invalidateAllCache_() {
    derivedCache_.reset();
    sourceAnalysis_.reset();
    derivedAnalysis_.reset();
}
void TimeSeriesSession::buildDerived_() const {
    if (!transform_) throw std::logic_error("No transform registered on this session");
    if (logger_)
        logger_->write(logging::Level::Debug, "building derived series for '" + seriesId_ + "'");
    derivedCache_ = std::make_shared<const TimeSeries>(transform_.value()(*source_));
}
}  // namespace analysis
