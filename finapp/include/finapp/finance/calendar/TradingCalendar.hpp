// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace finance {
using Timestamp = int64_t;
using TimestampsPtr = std::shared_ptr<std::vector<int64_t>>;

enum class RollConvention : std::uint8_t {
    Following,          //
    ModifiedFollowing,  //
    Preceding,          //
    ModifiedPreceding,  //
    Unadjusted          //
};

class TradingCalendar {
 public:
    virtual bool isTradingDay(Timestamp ts) = 0;
    virtual Timestamp roll(Timestamp ts, RollConvention c) = 0;
    virtual TimestampsPtr schedule(Timestamp start, Timestamp end) = 0;
    virtual double periodsPerYear(Timestamp start, Timestamp end) = 0;
};
}  // namespace finance
