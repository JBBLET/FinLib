// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "finapp/common/logger/ILogger.hpp"
#include "finapp/data/repository/interface/IFXRepository.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

using ts::InterpolationStrategy;
using ts::TimeSeries;
using ts::TimeSeriesService;
using ts::Timestamp;
using ts::Timestamps;
using ts::TimestampsPtr;
using ts::analysis::TimeSeriesSession;

namespace finapp {

class FXService {
 public:
    FXService(std::shared_ptr<TimeSeriesService> timeSeriesService, std::shared_ptr<IFXRepository> fxInfoRepository,
              finapp::logging::ILogger* logger = nullptr);

    TimeSeries load(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency, Timestamp fromMs,
                    Timestamp endMs, Timestamp frequencyMs,
                    InterpolationStrategy strategy = InterpolationStrategy::Nearest);

    // Overload sharing a caller-owned timestamp grid so multiple FX/asset series stay
    // pointer-aligned in downstream operators (PortfolioService::valueSeries).
    TimeSeries load(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency,
                    TimestampsPtr timestamps);

    double loadSingleFxAtTs(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency,
                            Timestamp ts);

    // Explicitly register an FX pair with a custom time-series ID.
    // Use this to override the default "<BASE><QUOTE>=X" yfinance convention,
    // e.g. when the price data comes from a different provider with its own naming.
    // If timeseriesId is empty, the default makePairId_ convention is used.
    // Overwrites any previously registered entry for the same pair.
    // TODO(JBBLET) Evaluate usage make it general to not by default save the yfinance naming convention but this should
    // only be define in the Provider with a set id convention in Finapp so only the FXProvider needs to create provider
    // conventions
    void registerPair(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency,
                      const std::string& timeseriesId = "");

    std::shared_ptr<TimeSeriesSession> createSession(const finance::Currency& base, const finance::Currency& quote,
                                                     Timestamp startMs, Timestamp endMs, Timestamp freqMs);

    std::shared_ptr<TimeSeriesSession> createSession(const finance::Currency& base, const finance::Currency& quote,
                                                     TimestampsPtr timestamps);

 private:
    std::shared_ptr<TimeSeriesService> timeSeriesService_;
    std::shared_ptr<IFXRepository> fxInfoRepository_;
    std::unique_ptr<finapp::logging::ILogger> logger_;

    // Canonical "<BASE><QUOTE>" id used as the TimeSeriesService seriesId and persisted
    // in FXInfos on first resolution of a new pair.
    // TODO(JBBLET) Evaluate usage make it general to not by default save the yfinance naming convention but this
    // should only be define in the Provider with a set id convention in Finapp so only the FXProvider needs to
    // create provider conventions
    static std::string makePairId_(const finance::Currency& base, const finance::Currency& quote);

    // Looks up an existing FXInfos or creates a new one (and persists it) by deriving
    // a pair id. Returns the resolved timeseriesID.
    std::string resolveSeriesId_(const finance::Currency& base, const finance::Currency& quote);
};

}  // namespace finapp
