# TimeSeriesService — Design

Status: **decisions locked (2026-06-28); implementation in progress.**
Scope: `finlib` data layer (`TimeSeriesService`, `CachedTimeSeriesRepository`, `CSVRepository`,
`InMemoryTimeSeriesRepository`, `StatsCore`, `TimeSeries`), with downstream effects on `finapp`
(`ReturnTransforms`, `AssetService`, `PortfolioService`).

This document records the decisions behind a refactor of `TimeSeriesService` and the repository layer.
It exists because the current service has grown three near-duplicate copies of the
cache→repository→provider waterfall and, more importantly, because the resampling it performs silently
biases analytical statistics.

---

## 0. Decisions locked

These are settled and drive the implementation plan in §11:

1. **`finlib` is finance-agnostic — the layering rule.** The repository and `TimeSeriesService` live in
   `finlib` (Domain A) and know nothing about markets. `SeriesKey` stays `{id, frequencyInMs}`, where
   `frequencyInMs` is a **generic sampling interval** (no finance meaning). Everything market-specific —
   the *Intraday/Daily* base-series split, the *M1/M5/M30/H1/D1* vocabulary, trading sessions/calendars, and
   **OHLC `BarSeries`** — lives in `finapp` (Domain B) and is expressed by composing finlib's generic
   `TimeSeries`. The "two canonical base series per ticker" is therefore a **finapp policy** (finapp only
   ever requests two `frequencyInMs` values), **not** a finlib key change. `SeriesKey` does not change.
2. **Single-value `TimeSeries` everywhere in finlib; OHLC deferred to finapp.** finlib stores/returns the
   existing single-value `TimeSeries` (one series = one value column). OHLC (§4) is a *finapp* concept built
   later as a struct-of-arrays of finlib `TimeSeries` sharing one `TimestampsPtr`.
3. **Full compute coverage.** Coverage is derived from the stored data's timestamps; we do **not** persist a
   fetch-history. Fetch policy is **boundary-only**: fetch the sub-ranges of `[start,end]` outside the stored
   `[min,max]` extent; interior holes are left as holes (NaN in analysis), not chased. Because coverage is
   just the extent, the existing `computeGaps(coverage, requested)` is reused unchanged.
4. **Repo-first build order.** `TimeSeriesService` and the repos are coupled through `SeriesKey`/coverage, so
   they ship together, authored repo-first. (The repo change is now small — computed coverage + real merge —
   since `SeriesKey` is unchanged.)

---

## 1. Motivation: the bias that started this

A portfolio NAV analysis produced suspiciously small mean/volatility versus the same metrics on the
underlying asset. Tracing it:

1. `PortfolioService::valueSeries` builds the NAV on a **calendar-day grid**
   (`makeRegularTimestamps(start, end, dayMs)`) — every day, weekends and holidays included.
2. Asset prices are aligned to that grid via `getResampled(..., Nearest)`, which **fills** weekend/holiday
   slots by copying the adjacent trading-day close.
3. `logReturns` then computes `log(vₜ/vₜ₋₁)`; a filled weekend point equals its neighbour, so the return
   is `log(1) = 0`. Roughly 2 of every 7 returns become spurious zeros.
4. `mean = Σr/n` and `var = Σ(r-μ)²/n` count **all** grid points, so `n` is inflated (~731 vs ~500 real
   trading days). Both mean and variance are deflated; annualizing by ×252 / ×√252 compounds the error.

**Root cause:** resampling-with-interpolation is a *presentation* concern that leaked into the *analysis*
path. Interpolated points are fabricated observations; statistics must never see them.

**Guiding principle:**

> Interpolation/fill is for **graphing**. Analysis runs on **real observations only**; missing points are
> represented explicitly (NaN) and excluded from the statistics, never fabricated.

---

## 2. Three storage tiers + computed coverage (finlib, generic)

Data flows through three distinct tiers, all explicit in the read/write logic.

| Tier | Component | Role | Lifetime |
|------|-----------|------|----------|
| 1. In-memory cache | `CachedTimeSeriesRepository.cache_` | fast repeat access | process |
| 2. Local database  | `inner_` repository (e.g. `CSVRepository`) | durable archive | persistent |
| 3. Provider        | `ITimeSeriesLoader` (e.g. YFinance) | external source, **tiered limits** | external |

Keep the **decorator** wiring: `CachedTimeSeriesRepository` wraps the DB, so its `load`/`coverage`/
`availableFrequencies` span tier 1 → tier 2 and `save`/`merge` write through to tier 2. The read/write
algorithm names all three tiers and **always persists provider results down to the DB**.

### The DB is the resolution authority, not the provider

`LoaderCapabilities` only answers *"if I fetch right now, what's the finest interval I can get for a window
of this age?"* But the **local DB can hold finer data than the provider will re-serve** for old ranges
(archived over time). So:

- **What resolution can I serve for this range?** → answered by the **DB** (+ provider-fillable boundary).
- **Capabilities** → consulted *only* to decide what a fetch can add.

### Coverage is computed (full compute, boundary-only)

`coverage(key)` derives the extent `[min(ts), max(ts)]` from the stored series' timestamps. No fetch-history
is persisted. `ensureNative_` fetches only the **prefix/suffix outside that extent**; interior holes are
left as holes and surface as NaN in analysis — never an error, never chased. Existing
`computeGaps(coverage, requested)` is reused since coverage is just the extent.

---

## 3. Generic resolution in finlib; canonical frequencies are a finapp policy

finlib stores series by `SeriesKey{id, frequencyInMs}` and treats `frequencyInMs` as an opaque sampling
interval. It enforces no particular set of intervals.

**finapp** owns the market semantics and the canonical model:

```cpp
// ---- finapp (Domain B), NOT finlib ----
enum class MarketDataKind { Intraday, Daily };           // the two base series finapp keeps per ticker
enum class BarFrequency  { M1, M5, M30, H1, D1 };        // request/derive vocabulary; maps to finlib freqMs
```

- finapp keeps **two base series per ticker** by only ever requesting two `frequencyInMs` values from finlib
  (an "intraday finest" and daily = `86_400_000`). The two-key discipline emerges from this policy; finlib
  needs no `MarketDataKind`.
- All other `BarFrequency` values are **derived on read** by coarsening the appropriate base within its
  family (Intraday `{M1,M5,M30,H1}`; Daily `{D1,W1,MO1}`). **1d is never derived from 1h.**
- **Achievability is data-driven** for intraday (the base is multi-resolution: ~1m recent, coarser older, so
  "finest derivable" varies along the range) and ordinal for daily.

The coarsening/derivation, family rules, and session-aware bucketing all live in finapp. finlib only offers
the generic primitives (raw load, align-to-grid, resample) that finapp composes.

---

## 4. OHLC (DEFERRED, finapp): struct-of-arrays of `TimeSeries`

OHLC is market data → a **finapp** concept, built later by composing finlib `TimeSeries`. Not in this round.

When it arrives, a bar series is a **struct of up to 5 `TimeSeries` sharing one `TimestampsPtr`** (SoA), not
an array-of-structs:

```cpp
// ---- finapp ----
struct BarSeries {
    ts::TimeSeries close;                                       // required
    std::optional<ts::TimeSeries> open, high, low, volume;     // engaged columns share close's TimestampsPtr
};
```

Why SoA-of-`TimeSeries`: columns **are** finlib `TimeSeries`, so projection is free (`bars.close` is already
a `TimeSeries`), finlib's `StatsCore`/resampling stay `TimeSeries`-only, and the shared `TimestampsPtr` is
exactly the alignment invariant finlib's operators require (`(high+low+close)*(1.0/3.0)` just works). Absent
columns are `std::optional` (an empty `TimeSeries` over a non-empty shared ptr violates the size invariant).

Per-column reduce/fill: open=first, high=max, low=min, close=last, volume=**sum**; for graphing only close
is interpolated, volume never. Bar aggregation (finapp, real bars, session-aware buckets) and
`TimeSeries::resampling` (finlib, interpolation for display) stay separate operations.

---

## 5. Analysis vs Graph: two finlib contracts

The generic finlib API differs only in how native data is placed onto a grid.

- **Graph (`getFilled`)** — may interpolate up from coarser data for a smooth, gap-free line. Cosmetic.
- **Analysis (`getRaw`/`getAligned`)** — real observations only; may **coarsen** but never **refine**;
  genuine holes become **NaN** and are excluded from stats; **throws** on Kind B (§6).

The single knob is the *coarsest acceptable interval* passed to the core: graph → "any" (interpolate);
analysis → "≤ requested" (coarser is a failure, not a fallback).

---

## 6. The achievability gate: two kinds of "missing"

**Kind A — holes at the correct resolution.** Real observations are at the requested interval; some slots
are empty (weekends/holidays; a missed archive week). → **Allowed.** Align with NaN; stats skip holes.

**Kind B — requesting finer than the data exists** (e.g. 1m over 6 months where only 1d/5m exists). The
"returns" between real points would not be 1m; spacing is meaningless. → **Reject (throw)** for analysis.

Gate (analysis): `available` = finest interval obtainable for the range from the **store** (+ fillable
boundary). `requested` finer than `available` → throw. Else allowed; coarsen if needed; remaining holes are
Kind A. Graphing never hits the gate.

---

## 7. NaN-aware statistics and returns

Single-asset analysis needs no NaN — analyze the native series directly (no calendar grid ⇒ no weekend slots
⇒ Fri→Mon is one daily return, `n` = real trading days). NaN-alignment is for **multi-asset** work sharing
one grid across differing calendars (a finapp concern, using finlib's `getAligned`).

- **Returns between consecutive *valid* observations** (finapp `ReturnTransforms`): emit a return only when
  spacing ≈ the analysis period; returns spanning a hole are **dropped**, not stitched. Stop injecting `0.0`.
- **`StatsCore` reducers are NaN-aware** (finlib, generic math): divide by valid-point count, not length;
  `mean = Σr / n_valid`, `var = Σ(r-μ)² / (n_valid[-1])`. `mean`/`var`/`std` first; `acf`/`autocov` over gaps
  restricted to gap-free views initially.
- **Two NaNs kept separate:** *bad-value* NaN from a provider is stripped at ingestion (`stripNaN`) so the DB
  stays clean; *structural* NaN ("didn't trade") is introduced only by `getAligned` and never persisted.

---

## 8. The core: `ensureNative_` (finlib, generic by freqMs)

One private method owns the tier 1→2→3 waterfall, including the **full-fetch-and-persist** path. Payload is
single-value `TimeSeries`.

```
ensureNative_(id, freqMs, start, end) -> TimeSeries:      // real points; holes possible

  // TIER 1+2: STORE (memory cache → local DB) via the decorator
  if store covers [start,end] for SeriesKey{id, freqMs}:  // computeGaps(computedCoverage, req) empty
      return store.load(key, start, end)

  partial = stored series for key (may be empty)

  // TIER 3: PROVIDER
  if no provider:
      if partial: return store.load(key, start, end)       // best effort; holes upstream
      throw "no provider and nothing stored for {id, freqMs}"

  if partial exists:
      gaps = computeGaps(computedCoverage(key), {start,end})   // boundary prefix/suffix only
      fetchAndMergeGaps_(key, gaps)                            // real union; persists to DB + cache
  else:
      fetched = stripNaN(provider.load(id, start, end, freqMs))   // provider clamps to what it can serve
      if fetched empty: throw
      store.save(key, fetched)                                // *** PERSIST to DB + cache (no cov arg) ***

  return store.load(key, start, end)                          // partial ⇒ real points with holes
```

`stripNaN` runs on ingestion so the DB never stores bad-value NaN.

---

## 9. Public API (finlib, generic)

```cpp
class TimeSeriesService {
 public:
  // ---- ANALYSIS (never interpolates) ----
  TimeSeries getRaw    (id, Timestamp freqMs, Timestamp start, Timestamp end);   // native, no grid; throws Kind B
  TimeSeries getAligned(id, Timestamp freqMs, TimestampsPtr grid);               // real on grid, NaN holes; throws Kind B

  // ---- GRAPHING (may interpolate up) ----
  TimeSeries getFilled (id, Timestamp freqMs, TimestampsPtr grid, InterpolationStrategy s = Nearest);
  TimeSeries getFilled (id, Timestamp freqMs, Timestamp start, Timestamp end, InterpolationStrategy s = Nearest);

  // ---- SPOT ----
  double     getSinglePoint(id, Timestamp ts);   // unchanged; age-tier provider windowing

  // ---- deprecated shims (keep callers compiling during migration) ----
  [[deprecated]] TimeSeries get(id, start, end, freqMs);
  [[deprecated]] TimeSeries getResampled(id, start, end, freqMs, strategy);
  [[deprecated]] TimeSeries get(id, TimestampsPtr grid);

 private:
  TimeSeries ensureNative_(id, Timestamp freqMs, Timestamp start, Timestamp end);  // THE waterfall
  // fetchAndMergeGaps_, computed-coverage helper retained
};
```

`getRaw` returns native; `getAligned` resamples onto the grid with `InterpolationStrategy::Exact` (NaN
holes); `getFilled` resamples with the chosen interpolation. finapp maps `BarFrequency`→`freqMs`, chooses the
grid, and does any coarsening/derivation on top. `getSinglePoint` stays separate (age-based windowing).

---

## 10. Capabilities, restated

`LoaderCapabilities` is purely a **"can a fetch improve on the store, and at what interval?"** oracle. It
never gates a request on its own and never bounds what can be *analyzed* — the DB does that. This is what
lets the finapp "fetch finest + save regularly" archive strategy work.

---

## 11. Implementation plan (repo-first)

Authored repo-first; finlib links green only once both repo and service are migrated (coupled through
coverage/save signatures). Each step maps to a task.

### Step A — `TimeSeries`: `InterpolationStrategy::Exact`
- Add `Exact` to the enum. In `partialWalk`, emit `NaN` where the target timestamp does not coincide with a
  source timestamp (targets before first / after last also NaN). Keep `isSynthetic_ = true`.

### Step B — repository: computed coverage + real merge (generic)
- `ITimeSeriesSaver`: `save(key, ts, cov)` → `save(key, ts)`; `doSave(key, ts)` (coverage computed on read).
  Keep the `isSynthetic` guard. (`SeriesKey` and `availableFrequencies` are unchanged — still generic.)
- `coverage(key)` in each repo derives `[min,max]` from stored timestamps.
- `InMemoryTimeSeriesRepository.doMerge` — **real union** (currently replaces; that's a bug). `CSVRepository`
  already unions (keep); drop its `.meta` writing/reading (coverage computed) or leave files unused.
- `CachedTimeSeriesRepository`: compute coverage from `cache_`/`inner_`; `doSave` drops `cov`.

### Step C — `TimeSeriesService` rewrite (generic by freqMs)
- `ensureNative_` (§8); `getRaw`/`getAligned`/`getFilled`; deprecated `get`/`getResampled` shims.
- Delete the duplicated waterfalls and the multi-frequency local-resample-from-coarser logic;
  `get(id, timestamps)`'s regular-spacing guard.

### Step D — `finlib` build checkpoint
- finlib (+ finlib tests `csv_repository_test`: drop `cov` arg from save calls) compiles and passes.

### Step E — `finapp` analysis routing + NaN
- `AssetService`: analysis session/`loadTimeSeriesValue` → `getRaw`/`getAligned`; graph → `getFilled`.
- `PortfolioService::valueSeries`: NAV on real trading-day / aligned grid, not a calendar grid.
- `ReturnTransforms`: stop injecting `0.0`; build returns between consecutive finite points (drop cross-gap).
- `StatsCore` reducers: NaN-aware counting (`mean`/`var`/`std` first) — finlib, generic.

### Step F — finapp canonical-frequency policy (optional this round)
- Introduce `MarketDataKind`/`BarFrequency` in finapp; map to finlib `freqMs`; enforce two-base discipline.
  Can be deferred — the bias fix in Step E does not require it.

### Step G — build + verify
- Build finlib + finapp + tests green; re-run the AAPL `finapp_returns_demo`; confirm mean/vol corrects.

---

## 12. Open questions / future

- **OHLC (§4, finapp)** — SoA `BarSeries`, per-column reducers, projections (`typicalPrice`). Deferred.
- **Interior backfill** — boundary-only is the current policy; a fetch-history (to safely chase fillable
  interior holes) is a possible later toggle.
- **Coverage threshold for analysis** — how many holes make a resolution "not really available"? For now:
  serve with NaN and report `(n_valid, n_expected, gap_count)`; caller decides.
- **Irregular-spacing variance** — multi-day gap returns carry more variance; ignored for daily, may
  normalize by `1/√Δt` later.
- **Session calendars** — finapp intraday bucketing needs a per-exchange session calendar; start with US
  equity.
</content>
Here's the full change map for the bias fix (Step E–G). The fill enters in two places: the analysis session source, and the NAV grid + prices.

Where the bias actually enters

AssetAnalysisService::createAnalysis(id, start,end,freq)
  → AssetService::createSession(start,end,freq)
    → ts::analysis::TimeSeriesSession(freq ctor)
      → service_->get(id, start,end,freq)        ← getFilled(Nearest) = WEEKEND FILL  ❌  (TimeSeriesSession.cpp:32)
PortfolioService::valueSeries
  → makeRegularTimestamps(start,end,dayMs)         ← CALENDAR grid                      ❌  (PortfolioService.cpp:184)
  → loadTimeSeriesValue(aid, timestamps) → get(grid) ← filled prices                    ❌  (AssetService.cpp:144)
ReturnTransforms::logReturns                        ← injects 0.0                        ❌  (ReturnTransforms.cpp:21)
StatsCore::mean/varianceFast                        ← divides by all points             ❌  (StatsCore.cpp)

The single most important one is the session source — ts::analysis::TimeSeriesSession is the analysis input, and it currently fills.

Change list by file

1. finlib/src/session/TimeSeriesSession.cpp — make the analysis source native/aligned (biggest fix)

This is ts::analysis::TimeSeriesSession — analysis-only, so it must never fill. Map its two constructors to the two analysis modes:
- Line 32 (freq ctor): service_->get(seriesId_, startMs_, endMs_, frequencyMs) → service_->getRaw(seriesId_, frequencyMs, startMs_, endMs_) (native; single-asset). This alone fixes the AAPL analysis: native trading days, correct n, no weekend zeros.
- Line 46 (grid ctor): service_->get(seriesId_, timestampsMs) → service_->getAligned(seriesId_, freq, timestampsMs) (NaN holes; multi-asset shared grid). Derive freq from the grid's min spacing.
- Line 80 (setFrequency): same swap as line 32 → getRaw.
- extendRange_ (line ~184): the frequencyMs_.has_value() branch still uses get() (filled) — switch it to getRaw too, to stay consistent with line 32. (The no-frequency branch already uses getRaw.)

Caveat: getRaw returns irregularly spaced native data (trading days). mean/var/std are fine, but acf/autocovariance/autocorrelation in TimeSeriesAnalysis/StatsCore assume regular spacing — they become approximate over weekends. Leave them for now (documented in design.md §12).

2. finapp/src/finance/analysis/ReturnTransforms.cpp — stop fabricating returns

- logReturns (line ~18-23) and simpleReturns (line ~33-38): remove the else returns.push_back(0.0). Build a return only between consecutive finite, positive observations; for a hole/NaN endpoint, push NaN (so NaN-aware stats exclude it) rather than 0.0.
- With the native source from #1 there are no weekend dups, so this mainly removes the non-positive-price 0.0 and makes the multi-asset NaN path correct.

3. finlib/src/core/StatsCore.cpp — NaN-aware reducers (needed once #1 grid ctor emits NaN)

- mean (line 19), varianceFast (line 33), and therefore standardDeviation: skip !std::isfinite(x) and divide by the finite count, not view.size(). TimeSeriesAnalysis::variance/standardDeviation already wrap these, so no change there.
- Leave skewness/kurtosis/acf/autocovariances for a later pass (they’re used less in the return path).
- Not needed for the single-asset getRaw path (no NaN there) — only for the getAligned multi-asset path.

4. finapp/src/service/PortfolioService.cpp — NAV on trading days, not calendar

valueSeries (line 182-260):
- Single-asset (covers your demo): build the grid from the asset's native trading-day timestamps (getRaw's timestamps) instead of makeRegularTimestamps(start,end,dayMs). Then every NAV point is a real trading day → logReturns gives one return per trading day.
- Multi-asset: grid = union of constituents' native trading days; value each day with prices aligned to that grid. This is the harder case (see gotcha below).
- The price load at line 218/AssetService.cpp:144 (get(grid) = filled) is fine for valuation (you need a price to value the book each day) — the correctness comes from the grid being trading days, not from how intra-grid prices are filled.

5. Verify: finapp/src/main.cpp demo

After the above, valueSeries(start,end,kDayMs) should be changed (or valueSeries internally ignores the calendar step and uses trading days). Re-run finapp_returns_demo; NAV mean/vol should jump up to match the asset’s ~252-day stats instead of being deflated by ~2/7.

The one real design decision (multi-asset NAV)

MultiTimeSeriesSession combines per-asset sessions with operator+/*. If sessions are getAligned (NaN holes) and you do q·price arithmetic, NaN propagates → NAV becomes NaN on any day an asset didn’t trade. So for multi-asset NAV you must decide: carry-forward last price for valuation (so NAV is defined every union-grid day) while still treating only real trading days as return observations. That split is the crux of multi-asset correctness — single-asset doesn’t hit it.

Suggested order

1. #1 freq-ctor → getRaw + #2 ReturnTransforms → fixes single-asset AAPL (no NaN machinery needed). Build + run demo.
2. #4 single-asset NAV grid → fixes the portfolio in the demo.
3. #3 StatsCore NaN + #1 grid-ctor → getAligned + #4 multi-asset → full multi-asset correctness.

Re-run after

- finapp_returns_demo (the demo numbers).
- finapp tests — portfolio_tracking_math_test, asset_service_test, portfolio_service_test may assume filled grids and need updating once the session goes native.

Want me to also dig into the MultiTimeSeriesSession NAV arithmetic (#4 multi-asset) so you have the exact carry-forward-vs-NaN spots before you start?
