// Copyright (c) 2026 JBBLET All Rights Reserved.
#pragma once

#include <string>
#include <utility>

#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"

class YFinanceProvider : public ITimeSeriesLoader {
 public:
    explicit YFinanceProvider(std::string pythonExec, std::string scriptPath)
        : python_(std::move(pythonExec)), scriptPath_(std::move(scriptPath)) {}
    TimeSeries load(const std::string& name, int64_t startTimestamp, int64_t endTimestamp) const override;
    LoaderCapabilities capabilities(const std::string& id) const override;

 private:
    std::string python_;
    std::string scriptPath_;
};
