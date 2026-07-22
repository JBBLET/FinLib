// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "finapp/finance/calendar/TradingCalendar.hpp"

namespace finance {

// A trading calendar with no holiday table: every Monday–Friday is a trading day.
// Enough for Monte-Carlo stepping and rolling recurrences; swap for an exchange
// calendar when per-market holidays matter.
class WeekdayCalendar : public TradingCalendar {
 public:
    bool isTradingDay(Timestamp ts) override {
        const auto wd = weekday_(ts);
        return wd != std::chrono::Saturday && wd != std::chrono::Sunday;
    }

    // Adjust a non-trading day onto a trading day per the convention. A day that is
    // already a trading day is returned unchanged.
    Timestamp roll(Timestamp ts, RollConvention c) override {
        switch (c) {
            case RollConvention::Unadjusted:
                return ts;
            case RollConvention::Following:
                return adjust_(ts, +1);
            case RollConvention::Preceding:
                return adjust_(ts, -1);
            case RollConvention::ModifiedFollowing: {
                const Timestamp r = adjust_(ts, +1);
                return sameMonth_(r, ts) ? r : adjust_(ts, -1);
            }
            case RollConvention::ModifiedPreceding: {
                const Timestamp r = adjust_(ts, -1);
                return sameMonth_(r, ts) ? r : adjust_(ts, +1);
            }
        }
        return ts;
    }

    // Day-aligned trading ticks in [start, end]. start is floored to its day boundary
    // so the grid lands on midnight-UTC ticks regardless of start's time-of-day.
    TimestampsPtr schedule(Timestamp start, Timestamp end) override {
        auto out = std::make_shared<std::vector<Timestamp>>();
        for (Timestamp d = floorDay_(start); d <= end; d += kMsPerDay_)
            if (isTradingDay(d)) out->push_back(d);
        return out;
    }

    double periodsPerYear(Timestamp start, Timestamp end) override {
        if (end <= start) return 0.0;
        const double years = static_cast<double>(end - start) / kMsPerYear_;
        return static_cast<double>(schedule(start, end)->size()) / years;
    }

 private:
    static constexpr Timestamp kMsPerDay_ = 86'400'000LL;
    static constexpr double kMsPerYear_ = 365.2425 * 86'400'000.0;

    static std::chrono::sys_days day_(Timestamp ts) {
        return std::chrono::floor<std::chrono::days>(
            std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{ts}});
    }
    static std::chrono::weekday weekday_(Timestamp ts) { return std::chrono::weekday{day_(ts)}; }
    static Timestamp floorDay_(Timestamp ts) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(day_(ts).time_since_epoch()).count();
    }
    static bool sameMonth_(Timestamp a, Timestamp b) {
        const std::chrono::year_month_day ya{day_(a)};
        const std::chrono::year_month_day yb{day_(b)};
        return ya.year() == yb.year() && ya.month() == yb.month();
    }
    Timestamp adjust_(Timestamp ts, int dir) {
        Timestamp t = ts;
        while (!isTradingDay(t)) t += dir * kMsPerDay_;
        return t;
    }
};
}  // namespace finance
