// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>

#include "finapp/data/repository/IFXRepository.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace finance {

class FXService {
 public:
    FXService(std::shared_ptr<TimeSeriesService> timeSeriesService, std::shared_ptr<IFXRepository> fxInfoRepository);
    TimeSeries load(const Currency& baseCurrency, const Currency& quoteCurrency, int64_t fromMs, int64_t endMs,
                    int64_t frequencyMs);

 private:
    std::shared_ptr<TimeSeriesService> timeSeriesService_;
    std::shared_ptr<IFXRepository> fxInfoRepository_;
};

}  // namespace finance
