# FinLib Architecture

## Overview

This repository contains two independent domains in a monorepo:

1. **FinLib (Domain A)** — a generic C++20 time series library. Reusable on any time-indexed data (weather, sensors, finance). Has zero knowledge of finance, no network dependencies, no Python runtime. Depends only on Eigen3.

2. **FinApp (Domain B)** — a finance analysis application that depends on FinLib. Contains data providers (YFinance), finance types (Asset, Portfolio), risk metrics, gRPC server, and UI. All external runtime dependencies (Python, TimescaleDB, network) live here.

**Guiding principles:**
- **Caller owns all configuration** — pure dependency injection, no config files, no singletons
- **Hard domain boundary** — FinLib has zero `#include` of anything in `finapp/`
- **Interfaces at every extension point** — all concrete implementations are replaceable

When a second project needs generic time series analysis, FinLib can be extracted to its own repo and loaded via `FetchContent` — all `target_link_libraries` calls remain unchanged because the CMake target names are stable.

---

## Directory Structure

```
include/finlib/                          # Domain A — public headers
├── analysis/
│   └── TimeSeriesAnalysis.hpp           # Cached statistical analysis facade
├── common/
│   ├── logger/
│   │   ├── ILogger.hpp                  # Abstract logger interface
│   │   ├── ConsoleLogger.hpp            # Console implementation
│   │   └── LogMacros.hpp                # LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DEBUG
│   └── utils/
│       └── TimeUtils.hpp                # ISO8601 parsing, timestamp conversion
├── core/
│   ├── TimeSeries.hpp                   # Immutable time series container
│   ├── TimeSeriesView.hpp               # Non-owning windowed view with lag support
│   └── StatsCore.hpp                    # Core stats: mean, variance, ACF, PACF, etc.
├── data/
│   ├── CoverageInfo.hpp                 # Metadata: coverage range, source, last update
│   ├── SeriesKey.hpp                    # Composite key (seriesId, frequencyMs) with hash
│   ├── TimeRange.hpp                    # Range struct with gap computation
│   ├── interfaces/
│   │   ├── ITimeSeriesLoader.hpp        # load() + capabilities()
│   │   ├── ITimeSeriesSaver.hpp         # save() + merge()
│   │   └── ITimeSeriesRepository.hpp    # Combines Loader + Saver + exists/coverage/frequencies
│   ├── implementation/
│   │   ├── CSVRepository.hpp            # File-based repository: <dir>/<id>/<freq>.csv
│   │   └── CachedTimeSeriesRepository.hpp  # In-memory cache decorator
│   └── services/
│       └── TimeSeriesService.hpp        # Orchestrates cache -> repository -> provider
├── models/
│   ├── interfaces/
│   │   ├── IModel.hpp                   # Abstract model interface
│   │   └── BaseModel.hpp                # Base: train/val/test split management
│   └── timeseries/regression/
│       └── ARModel.hpp                  # AR(q) with OLS, Yule-Walker, Levinson-Durbin
└── session/
    ├── AppContext.hpp                    # DI container: logger + repository
    └── ModelSession.hpp                 # Stateful online forecasting + drift detection

src/                                     # Domain A — implementations
├── CMakeLists.txt
├── analysis/TimeSeriesAnalysis.cpp
├── core/
│   ├── TimeSeries.cpp                   # Resampling, interpolation, operators
│   ├── TimeSeriesView.cpp               # Slice, shift, lag, regularity check
│   └── StatsCore.cpp
├── data/
│   ├── CSVRepository.cpp                # File I/O, .meta sidecar management
│   └── TimeSeriesService.cpp            # Gap detection, frequency resolution
├── models/
│   ├── interfaces/IModel.cpp
│   └── timeseries/regression/ARModel.cpp
├── session/ModelSession.cpp
└── utils/
    ├── TimeUtils.cpp
    └── LoggerUtils.cpp

finapp/                                  # Domain B — finance application
├── CMakeLists.txt
├── include/finapp/
│   └── providers/
│       └── YFinanceProvider.hpp         # ITimeSeriesLoader impl (Python/yfinance)
├── src/
│   └── providers/
│       └── YFinanceProvider.cpp
└── scripts/
    └── YFinance_loader.py               # Python script called by YFinanceProvider

tests/
├── unit_tests/                          # Domain A tests
│   ├── time_series_view_test.cpp
│   ├── time_series_resampling_test.cpp
│   ├── time_series_operation_test.cpp
│   ├── time_series_stats_test.cpp
│   ├── time_series_analysis_test.cpp
│   ├── ar_model_test.cpp
│   ├── csv_repository_test.cpp
│   └── model_session_test.cpp
└── finapp_tests/                        # Domain B tests
    └── test_yfinance_provider.cpp
```

---

## Module Dependency Graph

```
Domain A (FinLib) — no external runtime deps
═══════════════════════════════════════════

finlib_core          (Eigen3)
    |
    +-- finlib_analysis
    |       |
    |       +-- finlib_models
    |               |
    |               +-- finlib_session
    |
    +-- finlib_data


Domain B (FinApp) — depends on Domain A
═══════════════════════════════════════════

finapp_providers ──> finlib_data ──> finlib_core
        |
        +-- (future) finapp_finance ──> finlib_analysis, finlib_data
        |
        +-- (future) finapp_finance_data ──> finapp_finance, finlib_data
        |
        +-- (future) finapp_grpc ──> finapp_finance, finapp_providers
        |
        +-- (future) finapp_strategy ──> finapp_finance, finlib_models
```

### Domain A Libraries

| Library | Sources | Dependencies |
|---------|---------|-------------|
| `finlib_core` | TimeSeries, TimeSeriesView, StatsCore, TimeUtils | Eigen3 |
| `finlib_analysis` | TimeSeriesAnalysis | finlib_core |
| `finlib_data` | CSVRepository, TimeSeriesService | finlib_core |
| `finlib_models` | ARModel, IModel | finlib_analysis |
| `finlib_session` | ModelSession, LoggerUtils | finlib_models |

### Domain B Libraries

| Library | Sources | Dependencies |
|---------|---------|-------------|
| `finapp_providers` | YFinanceProvider | finlib_data |

---

## Core Module

### TimeSeries

Immutable container holding a shared timestamp vector and a values vector. Supports:
- Element-wise arithmetic (`+=`, `-=`, `*=`)
- `slice(start, length)` — returns a `TimeSeriesView`
- `view()` — full view of the series
- `resampling(newTimestamps, strategy)` — Linear, Stochastic, or Nearest interpolation

Timestamps are stored as `shared_ptr<const vector<int64_t>>`, enabling zero-copy sharing between a `TimeSeries` and its views.

### TimeSeriesView

Non-owning windowed view into a `TimeSeries`. Provides:
- `slice(start, length)`, `shift(offset)`
- `lag(steps)` — shifts the value index relative to timestamps
- `asEigenVector()` — direct conversion for linear algebra
- `checkRegularity()` — validates even spacing within tolerance

### StatsCore

Namespace `stats` containing pure functions: `mean`, `variance`, `skewness`, `kurtosis`, `autocovariance`, `ACF`, `PACF`, `toeplitzMatrix`.

---

## Analysis Module

### TimeSeriesAnalysis

Facade wrapping a `TimeSeriesView` with mutable cached results. Computes mean, variance, skewness, kurtosis, ACF, autocovariances, and Toeplitz matrices on demand, caching results until `invalidateCache()`.

Additional namespaces:
- `analysis::hypothesisTesting` — Jarque-Bera, ADF, Breusch-Pagan, Breusch-Godfrey
- `analysis::finance` — volatility

---

## Data Module

### Interfaces

```
ITimeSeriesLoader          ITimeSeriesSaver
├── load(id, start, end)   ├── save(SeriesKey, TimeSeries, CoverageInfo)
├── capabilities(id)       └── merge(SeriesKey, TimeSeries)
│
└───────────┬──────────────────────┘
            |
    ITimeSeriesRepository
    ├── exists(SeriesKey)
    ├── coverage(SeriesKey) -> optional<CoverageInfo>
    ├── availableFrequencies(id) -> vector<int64_t>
    ├── load(SeriesKey)
    └── load(SeriesKey, start, end)
```

### Key Types

- **`SeriesKey`** — composite key `{seriesId, frequencyInMs}` with `std::hash` specialization. Each (series, frequency) pair is independently tracked.
- **`CoverageInfo`** — records what time range is covered, by which source, and when it was last updated.
- **`TimeRange`** — simple `{startMs, endMs}` with `computeGaps(coverage, requested)` for gap detection.
- **`LoaderCapabilities`** — `{earliestAvailableMs, finestFrequencyMs}` reported by a data provider.

### Implementations (Domain A)

**CSVRepository** (`ITimeSeriesRepository`)
- Directory layout: `<baseDir>/<seriesId>/<frequencyMs>.csv`
- Metadata in `.meta` sidecar files (key=value format).
- `merge()` uses `std::map<int64_t, double>` for sorted deduplication.

**CachedTimeSeriesRepository** (`ITimeSeriesRepository`, decorator)
- Wraps an inner `ITimeSeriesRepository`.
- Maintains `unordered_map<SeriesKey, TimeSeries>` and `unordered_map<SeriesKey, CoverageInfo>` in memory.
- On cache miss, loads from inner and populates cache.
- On `merge()`, delegates to inner then repopulates cache from the merged result.
- Uses `filterByRange_()` with binary search (`lower_bound`/`upper_bound`) for efficient range extraction.

### Implementations (Domain B)

**YFinanceProvider** (`ITimeSeriesLoader`) — in `finapp/`
- Executes a Python script via `popen()`, parses CSV output.
- Reports daily frequency (86,400,000 ms) as its finest grain.
- Caller provides Python executable path and script path (no defaults).

**TimescaleDBTimeSeriesRepository** (planned, Phase 2) — will implement `ITimeSeriesRepository`.

### TimeSeriesService

Orchestrates the 3-layer data architecture: **cache -> repository -> provider**.

`get(id, startMs, endMs, requestedFrequencyMs)` resolves data in 4 steps:

1. **Exact key match in cache** — if cached and coverage is complete, return filtered slice.
2. **Finer frequency locally** — search `availableFrequencies()` for a finer grain that covers the range, resample to requested frequency.
3. **Partial coverage with gaps** — validate provider capabilities, fetch each gap, merge into repository.
4. **No local data** — full fetch from provider, save to cache and repository.

```
User Request
    |
    v
[Cache: exact key?] --yes--> [coverage complete?] --yes--> return
    |                              |
    no                           no (gaps)
    |                              |
    v                              v
[Cache: finer freq?]         [Provider: fetch gaps]
    |                              |
   yes --> resample & return       v
    |                         [Merge into repo]
    no                             |
    |                              v
    v                         return from cache
[Provider: full fetch]
    |
    v
[Save to repo + cache]
    |
    v
return
```

---

## Models Module

### Inheritance

```
IModel (abstract, enable_shared_from_this)
└── BaseModel
    └── ARModel
```

### IModel Interface

- `setData(view, trainRatio, validationRatio)` — configure data splits
- `fit()` — train the model
- `evaluate(view)` — compute regression/classification metrics
- `predictOneStep(window)` — single-step prediction from a context window
- `contextSize()` — required window length
- `createFresh()` — factory: returns unconfigured copy of same type
- `requiresRegularSpacing()`, `regularityTolerance()` — spacing constraints

### BaseModel

Manages train/validation/test splits from a `TimeSeriesView`. Caches a `TimeSeriesAnalysis` on the training set. Validates regularity requirements before fitting.

### ARModel

Autoregressive model AR(q) with three solvers:
- **OLS** — `X.colPivHouseholderQr().solve(Y)` on the lag matrix
- **Yule-Walker** — Toeplitz system from autocovariances
- **Levinson-Durbin** — recursive O(q^2) algorithm

Computes: `phi_` coefficients, `intercept_`, `sigmaEpsilon_`, covariance matrix, standard errors, t-statistics, p-values.

Prediction: `phi_.dot(window.reverse()) + intercept_`

---

## Session Module

### AppContext

Minimal DI container passed by reference:

```cpp
struct AppContext {
    logging::ILogger* logger;
    ITimeSeriesRepository* repository;
};
```

### ModelSession

Stateful online forecasting session. Lifecycle:

1. **Construction** — validates model is fitted, initializes sliding window from the tail of a `TimeSeriesView`.

2. **`forecast(steps)`** — multi-step ahead prediction. Uses a temporary copy of the window; the real window is not modified (allows repeated forecasts from the same state).

3. **`observe(value, timestamp)`** — receives actual values, computes prediction error, updates rolling MSE/MAE, slides the window forward with the actual value. Buffers observations in `writeBuffer_`.

4. **`shouldRefit(mseThreshold)`** — returns `true` if rolling MSE exceeds the threshold (concept drift detection).

5. **`refit(newView)`** — flushes buffer, calls `createFresh()` on the model, re-fits on new data, resets window.

6. **Destructor** — calls `flush_()` to persist buffered observations to the repository via `merge()`.

---

## Common / Utilities

### Logger

```
ILogger (abstract)
├── write(Level, msg)
└── ConsoleLogger -> std::cout
```

Used via macros: `LOG_INFO(context, msg)`, `LOG_WARN(...)`, `LOG_ERROR(...)`, `LOG_DEBUG(...)`

### TimeUtils

| Function | Purpose |
|----------|---------|
| `msToStringISO8601(ms)` | Unix ms -> ISO8601 string |
| `parseIso8601ToMs(str)` | ISO8601 -> Unix ms |
| `msToStringDate(ms)` | Unix ms -> YYYY-MM-DD |

---

## Design Patterns

| Pattern | Where | Purpose |
|---------|-------|---------|
| **Dependency Injection** | `AppContext` -> `ModelSession` | Decouple from concrete logger/repository |
| **Decorator** | `CachedTimeSeriesRepository` wraps `ITimeSeriesRepository` | Add caching transparently |
| **Strategy** | `InterpolationStrategy` enum | Pluggable resampling algorithms |
| **Template Method** | `BaseModel` -> `ARModel` | Reuse data split logic |
| **Factory** | `IModel::createFresh()` | Create blank model copies for re-fitting |
| **Interface Segregation** | `ITimeSeriesLoader` / `ITimeSeriesSaver` | Clients depend only on what they need |

---

## Build Configuration

- **Standard**: C++20
- **External deps**: Eigen3 (FetchContent), GoogleTest (FetchContent)
- **Build options**: `BUILD_TESTS` (ON), `BUILD_BENCHMARKS` (OFF), `BUILD_PYTHON` (ON)

### CMake Structure

```cmake
# Root CMakeLists.txt
add_subdirectory(src)      # FinLib (Domain A)
add_subdirectory(finapp)   # FinApp (Domain B)
add_subdirectory(tests)    # Both domains

# finapp/CMakeLists.txt
add_library(finapp_providers ...)
target_link_libraries(finapp_providers PUBLIC finlib_data)
```

To extract FinLib to a separate repo later, replace `add_subdirectory(src)` with `FetchContent(FinLib)` — all downstream `target_link_libraries` calls remain unchanged.

---

## Frequency-Keyed Storage

Each `(seriesId, frequencyMs)` pair is independently tracked. This solves the multi-grain coverage problem:

- Daily data Jan 1 - Feb 1 is stored at key `("AAPL", 86400000)`
- Hourly data Jan 15 - Feb 1 is stored at key `("AAPL", 3600000)`
- Each key has its own `CoverageInfo`, so capabilities are never overestimated.

`TimeSeriesService` can derive coarser frequencies from finer ones via resampling, but never the reverse.

---

## Planned Domain B Structure (Phases 1-4)

```
finapp/
├── include/finapp/
│   ├── providers/
│   │   └── YFinanceProvider.hpp              # Phase 1 (done)
│   ├── finance/
│   │   ├── Currency.hpp                      # Phase 1
│   │   ├── IAsset.hpp                        # Phase 1 (ETF, Equity, Bond, Cash)
│   │   ├── Transaction.hpp                   # Phase 1
│   │   └── Portfolio.hpp                     # Phase 1
│   ├── finance_data/
│   │   ├── interfaces/
│   │   │   ├── IPortfolioRepository.hpp      # Phase 1
│   │   │   └── ITransactionRepository.hpp    # Phase 1
│   │   ├── CSVFinanceRepository.hpp          # Phase 1
│   │   └── TimescaleDBRepository.hpp         # Phase 2
│   ├── risk/                                 # Phase 1
│   ├── grpc/                                 # Phase 1
│   ├── strategy/
│   │   ├── IStrategy.hpp                     # Phase 4
│   │   └── BacktestEngine.hpp                # Phase 4
│   └── python/                               # Phase 2 (pybind11 bridge)
└── scripts/
    └── YFinance_loader.py
```
