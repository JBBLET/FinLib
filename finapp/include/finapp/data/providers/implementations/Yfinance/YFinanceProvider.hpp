// Copyright (c) 2026 JBBLET All Rights Reserved.
#pragma once

#include <memory>
#include <string>

#include "finapp/common/logger/ILogger.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"

namespace finapp {

class YFinanceProvider : public ITimeSeriesLoader {
 public:
    YFinanceProvider(std::string pythonExec, std::string scriptPath, finapp::logging::ILogger* logger = nullptr);
    TimeSeries load(const std::string& name, Timestamp startTimestamp, Timestamp endTimestamp) const override;
    LoaderCapabilities capabilities(const std::string& id) const override;

 private:
    std::string python_;
    std::string scriptPath_;
    std::unique_ptr<finapp::logging::ILogger> logger_;
};

}  // namespace finapp
