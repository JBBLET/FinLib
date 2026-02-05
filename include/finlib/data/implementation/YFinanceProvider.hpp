#pragma once
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"

class YFinanceProvider : public ITimeSeriesLoader {

public:
  explicit YFinanceProvider(
      std::string python_exec ="/home/jbblet/.venvs/finlib-python/bin/python",
      std::string script_path ="src/scripts/YFinance_loader.py"
      ): python_(std::move(python_exec)), script_path_(std::move(script_path)){}
  TimeSeries load(
      const std::string& name,
      int64_t start_ts,
      int64_t end_ts
      ) override;

private:
  std::string python_;
  std::string script_path_;
};
