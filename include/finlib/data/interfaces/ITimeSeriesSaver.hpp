#pragma once
#include "finlib/core/TimeSeries.hpp"
#include <string>

class ITimeSeriesSaver{

public:
  virtual ~ITimeSeriesSaver() = default;

  virtual void save(
      const std::string& name,
      const TimeSeries& ts
  ) = 0;
};
