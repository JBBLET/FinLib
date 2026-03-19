// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"
#include "finlib/data/interfaces/ITimeSeriesSaver.hpp"

class ITimeSeriesRepository : public ITimeSeriesLoader, public ITimeSeriesSaver {};
