#pragma once
#include "finlib/data/interfaces/ItimeSeriesLoader.hpp"
#include "finlib/data/interfaces/ITimeSeriesSaver.hpp"

class ITimeSeriesRepository: public ItimeSeriesLoader, public ITimeSeriesSaver{};
