// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/service/FXService.hpp"

#include <memory>
#include <string>
#include <utility>

#include "finapp/common/Exception.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {

using finance::Currency;

FXService::FXService(std::shared_ptr<TimeSeriesService> timeSeriesService,
                     std::shared_ptr<IFXRepository> fxInfoRepository, finapp::logging::ILogger* logger)
    : timeSeriesService_(std::move(timeSeriesService)),
      fxInfoRepository_(std::move(fxInfoRepository)),
      logger_(finapp::logging::PrefixedLogger::wrap(logger, "FXService")) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

TimeSeries FXService::load(const Currency& baseCurrency, const Currency& quoteCurrency, Timestamp fromMs,
                           Timestamp endMs, Timestamp frequencyMs, InterpolationStrategy strategy) {
    if (baseCurrency == quoteCurrency) {
        return ts::common::utils::timeSeries::generateConstantTimeSeries(
            makePairId_(baseCurrency, quoteCurrency), fromMs, endMs, frequencyMs, 1.0);
    }

    const std::string seriesId = resolveSeriesId_(baseCurrency, quoteCurrency);
    return timeSeriesService_->getResampled(seriesId, fromMs, endMs, frequencyMs, strategy);
}

TimeSeries FXService::load(const Currency& baseCurrency, const Currency& quoteCurrency, TimestampsPtr timestamps) {
    if (!timestamps) {
        throw finapp::InvalidArgument("FXService::load: timestamps pointer is null.");
    }

    if (baseCurrency == quoteCurrency) {
        return ts::common::utils::timeSeries::generateConstantTimeSeries(
            makePairId_(baseCurrency, quoteCurrency), std::move(timestamps), 1.0);
    }

    const std::string seriesId = resolveSeriesId_(baseCurrency, quoteCurrency);
    return timeSeriesService_->get(seriesId, std::move(timestamps));
}
double FXService::loadSingleFxAtTs(const Currency& baseCurrency, const Currency& quoteCurrency, Timestamp ts) {
    if (baseCurrency == quoteCurrency) {
        return 1.0;
    }

    const std::string seriesId = resolveSeriesId_(baseCurrency, quoteCurrency);
    return timeSeriesService_->getSinglePoint(seriesId, ts);
}

std::shared_ptr<::TimeSeriesSession> FXService::createSession(const Currency& base, const Currency& quote,
                                                              Timestamp startMs, Timestamp endMs,
                                                              Timestamp frequencyMs) {
    return std::make_shared<::TimeSeriesSession>(
        timeSeriesService_, makePairId_(base, quote), startMs, endMs, frequencyMs);
}

std::shared_ptr<::TimeSeriesSession> FXService::createSession(const Currency& base, const Currency& quote,
                                                              TimestampsPtr timestamps) {
    return std::make_shared<::TimeSeriesSession>(timeSeriesService_, makePairId_(base, quote), std::move(timestamps));
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
        logger_->write(
            finapp::logging::Level::Debug,
            "resolveSeriesId_: new pair " + toString(base) + "/" + toString(quote) + " -> " + info.timeseriesID);
    fxInfoRepository_->save(info);
    return info.timeseriesID;
}

}  // namespace finapp
