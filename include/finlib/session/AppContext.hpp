// Copyright 2026 JBBLET
#pragma once

#include "finlib/common/logger/ILogger.hpp"
#include "finlib/data/interfaces/ITimeSeriesRepository.hpp"

struct AppContext {
    logging::ILogger* logger;
    ITimeSeriesRepository* repository;
};
