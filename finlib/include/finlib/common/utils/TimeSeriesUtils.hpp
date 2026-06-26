// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <string>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace ts::common::utils::timeSeries {

// Builds a regular timestamp grid [beginMs, beginMs + k*frequencyMs, ...] with
// the largest k such that the tick stays <= endMs. Shared by callers that want
// multiple TimeSeries aligned on the same timestamp vector (zero-copy sharing).
TimestampsPtr makeRegularTimestamps(Timestamp beginMs, Timestamp endMs, Timestamp frequencyMs);

// Constant-valued TimeSeries on a freshly built regular grid.
TimeSeries generateConstantTimeSeries(const std::string& id, Timestamp beginMs, Timestamp endMs, Timestamp frequencyMs,
                                      double value = 1.0);

// Constant-valued TimeSeries sharing the caller's timestamp vector.
TimeSeries generateConstantTimeSeries(const std::string& id, TimestampsPtr timestamps, double value = 1.0);

}  // namespace ts::common::utils::timeSeries
