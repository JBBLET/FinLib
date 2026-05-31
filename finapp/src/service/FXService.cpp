// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/service/FXService.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "finapp/finance/common/Currency.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace finapp {

using namespace finance;

FXService::FXService(std::shared_ptr<TimeSeriesService> timeSeriesService,
                     std::shared_ptr<IFXRepository> fxInfoRepository, finapp::logging::ILogger* logger)
    : timeSeriesService_(std::move(timeSeriesService)),
      fxInfoRepository_(std::move(fxInfoRepository)),
      logger_(finapp::logging::PrefixedLogger::wrap(logger, "FXService")) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

TimeSeries FXService::load(const Currency& baseCurrency, const Currency& quoteCurrency, int64_t fromMs, int64_t endMs,
                           int64_t frequencyMs, InterpolationStrategy strategy) {
    if (baseCurrency == quoteCurrency) {
        return common::utils::timeSeries::generateConstantTimeSeries(
            makePairId_(baseCurrency, quoteCurrency), fromMs, endMs, frequencyMs, 1.0);
    }

    const std::string seriesId = resolveSeriesId_(baseCurrency, quoteCurrency);
    return timeSeriesService_->getResampled(seriesId, fromMs, endMs, frequencyMs, strategy);
}

TimeSeries FXService::load(const Currency& baseCurrency, const Currency& quoteCurrency, TimestampPtr timestamps) {
    if (!timestamps) {
        throw std::invalid_argument("FXService::load: timestamps pointer is null.");
    }

    if (baseCurrency == quoteCurrency) {
        return common::utils::timeSeries::generateConstantTimeSeries(
            makePairId_(baseCurrency, quoteCurrency), std::move(timestamps), 1.0);
    }

    const std::string seriesId = resolveSeriesId_(baseCurrency, quoteCurrency);
    return timeSeriesService_->get(seriesId, std::move(timestamps));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string FXService::makePairId_(const Currency& base, const Currency& quote) {
    // yfinance FX ticker convention: "EURUSD=X"
    return toString(base) + toString(quote) + "=X";
}

void FXService::registerPair(const Currency& baseCurrency, const Currency& quoteCurrency,
                             const std::string& timeseriesId) {
    const std::string id = timeseriesId.empty() ? makePairId_(baseCurrency, quoteCurrency) : timeseriesId;
    fxInfoRepository_->save(FXInfos{baseCurrency, quoteCurrency, id});
}

std::string FXService::resolveSeriesId_(const Currency& base, const Currency& quote) {
    if (fxInfoRepository_->exists(base, quote)) {
        return fxInfoRepository_->load(base, quote).timeseriesID;
    }

    // First-time resolution: derive the id by convention and persist the metadata so
    // future calls take the fast path. The actual price history will be fetched from
    // the provider lazily on the next TimeSeriesService::get call.
    FXInfos info{base, quote, makePairId_(base, quote)};
    if (logger_)
        logger_->write(finapp::logging::Level::Debug,
                       "resolveSeriesId_: new pair " + toString(base) + "/" + toString(quote) + " -> " +
                           info.timeseriesID);
    fxInfoRepository_->save(info);
    return info.timeseriesID;
}

}  // namespace finapp
