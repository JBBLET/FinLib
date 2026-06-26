// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

namespace ts {
class TimeSeries;
}  // namespace ts

namespace finance::analysis {

// Derived-series transforms shared by per-asset and portfolio (NAV) analyses.

// Log returns: r_t = ln(p_t / p_{t-1}). Non-positive inputs would make the
// logarithm undefined, so those steps are emitted as 0 (NAV is a positive index).
ts::TimeSeries logReturns(const ts::TimeSeries& series);

// Simple (arithmetic) returns: r_t = (p_t - p_{t-1}) / p_{t-1}.
ts::TimeSeries simpleReturns(const ts::TimeSeries& series);

}  // namespace finance::analysis
