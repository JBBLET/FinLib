// Copyright 2026 JBBLET
#pragma once

#include "finlib/common/logger/ILogger.hpp"
#include "finlib/data/interfaces/ITimeSeriesSaver.hpp"

struct AppContext {
    logging::ILogger* logger_;
    ITimeSeriesSaver* saver_;
};
