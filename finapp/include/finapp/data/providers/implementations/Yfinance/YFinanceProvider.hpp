// Copyright (c) 2026 JBBLET All Rights Reserved.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"

using ts::LoaderCapabilities;
using ts::TimeSeries;
using ts::Timestamp;

namespace finapp {
class YFinanceProvider : public ts::ITimeSeriesLoader {
 public:
    explicit YFinanceProvider();
    TimeSeries load(const std::string& name, Timestamp startTimestamp, Timestamp endTimestamp,
                    std::optional<Timestamp> requestedFrequency = std::nullopt) const override;
    LoaderCapabilities capabilities(const std::string& id) const override;

 private:
};

}  // namespace finapp
