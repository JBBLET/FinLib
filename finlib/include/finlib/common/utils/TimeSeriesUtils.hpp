// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cstdint>
#include <string>

#include "finlib/core/TimeSeries.hpp"

namespace common::utils::timeSeries {

// Builds a regular timestamp grid [beginMs, beginMs + k*frequencyMs, ...] with
// the largest k such that the tick stays <= endMs. Shared by callers that want
// multiple TimeSeries aligned on the same timestamp vector (zero-copy sharing).
TimestampPtr makeRegularTimestamps(int64_t beginMs, int64_t endMs, int64_t frequencyMs);

// Constant-valued TimeSeries on a freshly built regular grid.
TimeSeries generateConstantTimeSeries(const std::string& id, int64_t beginMs, int64_t endMs, int64_t frequencyMs,
                                      double value = 1.0);

// Constant-valued TimeSeries sharing the caller's timestamp vector.
TimeSeries generateConstantTimeSeries(const std::string& id, TimestampPtr timestamps, double value = 1.0);

}  // namespace common::utils::timeSeries
