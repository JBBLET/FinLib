// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <chrono>

#include "finapp/finance/calendar/CalendarBase.hpp"

namespace finance {

// A trading calendar with no holiday table: every Monday–Friday is a trading day.
// Enough for Monte-Carlo stepping and rolling recurrences; use HolidayCalendar when
// per-market holidays matter. roll/schedule/periodsPerYear come from CalendarBase.
class WeekdayCalendar : public CalendarBase {
 public:
    WeekdayCalendar() : CalendarBase(0) {}

    bool isTradingDay(Timestamp ts) const override {
        const auto wd = localWeekday_(ts);
        return wd != std::chrono::Saturday && wd != std::chrono::Sunday;
    }
};
}  // namespace finance
