// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace finance {
using Timestamp = int64_t;
// Same type as ts::TimestampsPtr (const inner vector).
using TimestampsPtr = std::shared_ptr<const std::vector<int64_t>>;

enum class RollConvention : std::uint8_t {
    Following,          //
    ModifiedFollowing,  //
    Preceding,          //
    ModifiedPreceding,  //
    Unadjusted          //
};

class TradingCalendar {
 public:
    virtual ~TradingCalendar() = default;

    // Is this instant a trading day on this calendar? The single point of variation between
    // calendars — roll/schedule/periodsPerYear are all expressed in terms of it (see CalendarBase).
    virtual bool isTradingDay(Timestamp ts) const = 0;

    // Adjust a non-trading instant onto a trading day per the convention (Unadjusted returns ts).
    virtual Timestamp roll(Timestamp ts, RollConvention c) const = 0;

    // Trading ticks in [start, end], one per trading day, ascending.
    virtual TimestampsPtr schedule(Timestamp start, Timestamp end) const = 0;

    // Trading days per year over [start, end] — the annualization factor (replaces hardcoded 252).
    virtual double periodsPerYear(Timestamp start, Timestamp end) const = 0;
};
}  // namespace finance
