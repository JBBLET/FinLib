// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>

#include "finapp/data/repository/interface/IFXRepository.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace finance {

class FXService {
 public:
    FXService(std::shared_ptr<TimeSeriesService> timeSeriesService, std::shared_ptr<IFXRepository> fxInfoRepository);

    TimeSeries load(const Currency& baseCurrency, const Currency& quoteCurrency, int64_t fromMs, int64_t endMs,
                    int64_t frequencyMs);

    // Overload sharing a caller-owned timestamp grid so multiple FX/asset series stay
    // pointer-aligned in downstream operators (PortfolioService::valueSeries).
    TimeSeries load(const Currency& baseCurrency, const Currency& quoteCurrency, TimestampPtr timestamps);

 private:
    std::shared_ptr<TimeSeriesService> timeSeriesService_;
    std::shared_ptr<IFXRepository> fxInfoRepository_;

    // Canonical "<BASE><QUOTE>" id used as the TimeSeriesService seriesId and persisted
    // in FXInfos on first resolution of a new pair.
    static std::string makePairId_(const Currency& base, const Currency& quote);

    // Looks up an existing FXInfos or creates a new one (and persists it) by deriving
    // a pair id. Returns the resolved timeseriesID.
    std::string resolveSeriesId_(const Currency& base, const Currency& quote);
};

}  // namespace finance
