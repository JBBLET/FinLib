// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>

#include "finapp/data/repository/interface/IFXRepository.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace finapp {

class FXService {
 public:
    FXService(std::shared_ptr<TimeSeriesService> timeSeriesService, std::shared_ptr<IFXRepository> fxInfoRepository);

    TimeSeries load(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency, int64_t fromMs, int64_t endMs,
                    int64_t frequencyMs);

    // Overload sharing a caller-owned timestamp grid so multiple FX/asset series stay
    // pointer-aligned in downstream operators (PortfolioService::valueSeries).
    TimeSeries load(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency, TimestampPtr timestamps);

    // Explicitly register an FX pair with a custom time-series ID.
    // Use this to override the default "<BASE><QUOTE>=X" yfinance convention,
    // e.g. when the price data comes from a different provider with its own naming.
    // If timeseriesId is empty, the default makePairId_ convention is used.
    // Overwrites any previously registered entry for the same pair.
    void registerPair(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency,
                      const std::string& timeseriesId = "");

 private:
    std::shared_ptr<TimeSeriesService> timeSeriesService_;
    std::shared_ptr<IFXRepository> fxInfoRepository_;

    // Canonical "<BASE><QUOTE>" id used as the TimeSeriesService seriesId and persisted
    // in FXInfos on first resolution of a new pair.
    static std::string makePairId_(const finance::Currency& base, const finance::Currency& quote);

    // Looks up an existing FXInfos or creates a new one (and persists it) by deriving
    // a pair id. Returns the resolved timeseriesID.
    std::string resolveSeriesId_(const finance::Currency& base, const finance::Currency& quote);
};

}  // namespace finapp
