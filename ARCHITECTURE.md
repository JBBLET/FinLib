# FinLib Architecture

## Overview

This repository is a monorepo with two peer domains:

1. **FinLib (Domain A)** — a generic C++20 time series library. Reusable on any time-indexed data (weather, sensors, finance). Has zero knowledge of finance, no network dependencies, no Python runtime. Depends only on Eigen3.

2. **FinApp (Domain B)** — a finance analysis application that depends on FinLib. Contains data providers (YFinance via pybind11), finance types (Asset, Portfolio), risk metrics, gRPC server, and UI. All external runtime dependencies (Python, TimescaleDB, network) live here.

**Guiding principles:**
- **Caller owns all configuration** — pure dependency injection, no config files, no singletons
- **Hard domain boundary** — FinLib has zero `#include` of anything in `finapp/`
- **Interfaces at every extension point** — all concrete implementations are replaceable
- **Namespace separation** — `finance::` holds pure domain types; `finapp::` holds all infrastructure (repositories, providers, services, importers)

When a second project needs generic time series analysis, FinLib can be extracted to its own repo and loaded via `FetchContent` — all `target_link_libraries` calls remain unchanged because the CMake target names are stable.

---

## Directory Structure

```
finlib/                                     # Domain A — time series library
├── CMakeLists.txt
├── include/finlib/                         # Public headers
│   ├── analysis/
│   │   └── TimeSeriesAnalysis.hpp          # Cached statistical analysis facade
│   ├── common/
│   │   ├── logger/
│   │   │   ├── ILogger.hpp                 # Abstract logger interface
│   │   │   ├── ConsoleLogger.hpp           # Console implementation
│   │   │   └── LogMacros.hpp               # LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DEBUG
│   │   └── utils/
│   │       └── TimeUtils.hpp               # ISO8601 parsing, timestamp conversion
│   ├── core/
│   │   ├── TimeSeries.hpp                  # Immutable time series container
│   │   ├── TimeSeriesView.hpp              # Non-owning windowed view with lag support
│   │   └── StatsCore.hpp                   # Core stats: mean, variance, ACF, PACF, etc.
│   ├── data/
│   │   ├── CoverageInfo.hpp                # Metadata: coverage range, source, last update
│   │   ├── SeriesKey.hpp                   # Composite key (seriesId, frequencyMs) with hash
│   │   ├── TimeRange.hpp                   # Range struct with gap computation
│   │   ├── interfaces/
│   │   │   ├── ITimeSeriesLoader.hpp       # load() + capabilities()
│   │   │   ├── ITimeSeriesSaver.hpp        # save() + merge()
│   │   │   └── ITimeSeriesRepository.hpp   # Combines Loader + Saver + exists/coverage/frequencies
│   │   ├── implementation/
│   │   │   ├── CSVRepository.hpp           # File-based repository: <dir>/<id>/<freq>.csv
│   │   │   └── CachedTimeSeriesRepository.hpp  # In-memory cache decorator
│   │   └── services/
│   │       └── TimeSeriesService.hpp       # Orchestrates cache -> repository -> provider
│   ├── models/
│   │   ├── interfaces/
│   │   │   ├── IModel.hpp                  # Abstract base: name, fit, isFitted, regularity, contextSize
│   │   │   ├── IRegressionModel.hpp        # Univariate regression: setData, predictOneStep, evaluate, createFresh
│   │   │   ├── IMultivariateRegressionModel.hpp  # Multi-input regression (planned)
│   │   │   ├── IProbabilisticModel.hpp     # Distribution prediction (planned)
│   │   │   ├── IClassificationModel.hpp    # Class prediction (planned)
│   │   │   ├── EvaluationResult.hpp        # RegressionEvaluation + ClassificationEvaluation structs
│   │   │   └── BaseRegressionModel.hpp     # Base: train/val/test split management for regression
│   │   └── timeseries/regression/
│   │       └── ARModel.hpp                 # AR(q) with OLS, Yule-Walker, Levinson-Durbin
│   └── session/
│       ├── AppContext.hpp                   # DI container: logger + saver
│       └── ModelSession.hpp                # Stateful online forecasting + drift detection
└── src/                                    # Implementations
    ├── analysis/TimeSeriesAnalysis.cpp
    ├── core/
    │   ├── TimeSeries.cpp                  # Resampling, interpolation, operators
    │   ├── TimeSeriesView.cpp              # Slice, shift, lag, regularity check
    │   └── StatsCore.cpp
    ├── data/
    │   ├── CSVRepository.cpp               # File I/O, .meta sidecar management
    │   └── TimeSeriesService.cpp           # Gap detection, frequency resolution
    ├── models/
    │   ├── interfaces/EvaluationResult.cpp # RegressionEvaluation + ClassificationEvaluation impl
    │   └── timeseries/regression/ARModel.cpp
    ├── session/ModelSession.cpp
    └── utils/
        ├── TimeUtils.cpp
        └── LoggerUtils.cpp

finapp/                                     # Domain B — finance application
├── CMakeLists.txt
├── include/finapp/
│   ├── data/
│   │   ├── importers/
│   │   │   └── YahooFinanceImporter.hpp    # Parse Yahoo Finance CSV exports → Transaction list
│   │   ├── providers/
│   │   │   ├── interfaces/
│   │   │   │   └── IAssetProviders.hpp     # Abstract asset metadata fetcher per AssetType
│   │   │   └── implementations/
│   │   │       └── Yfinance/
│   │   │           ├── YFinanceProvider.hpp        # ITimeSeriesLoader impl (pybind11/yfinance)
│   │   │           ├── YFinanceEquityProvider.hpp  # IAssetProvider impl for equity metadata
│   │   │           └── YfinanceUtils.hpp           # PythonRuntime singleton (pybind11 guard)
│   │   └── repository/
│   │       ├── interface/
│   │       │   ├── IAssetRepository.hpp    # CRUD for asset metadata per AssetType
│   │       │   ├── IPortfolioRepository.hpp # Snapshots + transaction log + soft delete
│   │       │   └── IFXRepository.hpp       # FX pair → timeseriesID + FXInfos struct
│   │       └── implementation/
│   │           └── CsvRepository/
│   │               ├── CSVEquityRepository.hpp
│   │               ├── CSVCashRepository.hpp
│   │               ├── CSVFXRepository.hpp
│   │               └── CSVPortfolioRepository.hpp  # Soft delete: rename to .csv.deleted
│   ├── finance/                            # Pure domain types — namespace finance::
│   │   ├── common/
│   │   │   ├── Currency.hpp                # Currency enum + string conversion
│   │   │   └── AssetId.hpp
│   │   ├── asset/
│   │   │   ├── IAsset.hpp                  # Abstract asset + Position struct
│   │   │   ├── Equity.hpp                  # Stock: ticker, exchange, sector
│   │   │   ├── ETF.hpp                     # ETF: expenseRatio, trackingIndex (planned)
│   │   │   ├── Bond.hpp                    # Bond: coupon, maturity, faceValue (planned)
│   │   │   └── Cash.hpp                    # Cash: denomination only, no price series
│   │   └── portfolio/
│   │       ├── Transaction.hpp             # Buy/Sell/Deposit/Withdrawal/Dividend/Split
│   │       ├── Portfolio.hpp               # Portfolio + Builder, positions + target allocations
│   │       └── PortfolioSnapshot.hpp       # Point-in-time snapshot (positions + cash balances)
│   └── service/                            # Infrastructure services — namespace finapp::
│       ├── AssetService.hpp                # Dispatches to per-type repos/providers + TimeSeriesService
│       ├── PortfolioService.hpp            # Reconstruct, save, compute value/weight series; createNew/delete
│       └── FXService.hpp                   # FX rate lookup + pair registration; yfinance "<BASE><QUOTE>=X" ids
├── src/
│   ├── data/
│   │   ├── importers/YahooFinanceImporter.cpp
│   │   ├── providers/Yfinance/
│   │   │   ├── YFinanceProvider.cpp
│   │   │   └── YFinanceEquityProvider.cpp
│   │   └── repository/implementation/CsvRepository/
│   │       ├── CSVEquityRepository.cpp
│   │       ├── CSVCashRepository.cpp
│   │       ├── CSVFXRepository.cpp
│   │       └── CSVPortfolioRepository.cpp
│   ├── finance/portfolio/Portfolio.cpp
│   └── service/
│       ├── AssetService.cpp
│       ├── FXService.cpp
│       └── PortfolioService.cpp
└── scripts/
    └── YFinanceFetcher.py                  # pybind11 Python module: fetch_ohlcv, fetch_equity_info, equity_exists

tests/
├── unit_tests/                             # Domain A tests
│   ├── time_series_view_test.cpp
│   ├── time_series_resampling_test.cpp
│   ├── time_series_operation_test.cpp
│   ├── time_series_stats_test.cpp
│   ├── time_series_analysis_test.cpp
│   ├── ar_model_test.cpp
│   ├── csv_repository_test.cpp
│   └── model_session_test.cpp
└── finapp_tests/                           # Domain B tests
    ├── test_yfinance_provider.cpp          # Integration test (hits real yfinance)
    ├── csv_equity_repository_test.cpp
    ├── csv_cash_repository_test.cpp
    ├── csv_fx_repository_test.cpp
    ├── csv_portfolio_repository_test.cpp
    ├── fx_service_test.cpp
    ├── asset_service_test.cpp
    ├── portfolio_service_test.cpp
    └── support/service_test_fakes.hpp      # In-memory fakes shared across service tests
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

                      finlib_core
                      /         \
               finlib_data    finlib_analysis
                  |
          finapp_core (Portfolio, domain types)
         /           \
finapp_providers   finapp_csv_repository
(YFinanceProvider,  (CSV impls of all
 YFinanceEquity     IRepository interfaces)
 Provider)     \
                \   finapp_importers
                 \  (YahooFinanceImporter)
                  \
           finapp_service (AssetService, FXService, PortfolioService)
```

### Domain A Libraries

| Library | Sources | Dependencies |
|---------|---------|-------------|
| `finlib_core` | TimeSeries, TimeSeriesView, StatsCore, TimeUtils | Eigen3 |
| `finlib_analysis` | TimeSeriesAnalysis | finlib_core |
| `finlib_data` | CSVRepository, TimeSeriesService | finlib_core |
| `finlib_models` | ARModel, EvaluationResult | finlib_analysis |
| `finlib_session` | ModelSession, LoggerUtils | finlib_models |

### Domain B Libraries

| Library | Sources | Dependencies |
|---------|---------|-------------|
| `finapp_core` | Portfolio, Transaction, Asset types | finlib_core |
| `finapp_providers` | YFinanceProvider, YFinanceEquityProvider | finlib_data, pybind11 |
| `finapp_csv_repository` | CSVEquity/Cash/FX/PortfolioRepository | finapp_core |
| `finapp_importers` | YahooFinanceImporter | finapp_core, finlib_core |
| `finapp_service` | AssetService, PortfolioService, FXService | finapp_core, finlib_data |

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
- Calls `YFinanceFetcher.fetch_ohlcv(symbol, start, end, "1d")` via pybind11 in-process.
- Returns adjusted close prices (`auto_adjust=True`; split- and dividend-adjusted).
- Reports daily frequency (86,400,000 ms) as its finest grain.

**TimescaleDBTimeSeriesRepository** (planned, Phase 2) — will implement `ITimeSeriesRepository`.

### TimeSeriesService

Orchestrates the 3-layer data architecture: **cache -> repository -> provider**.

Two entry points:
- `get(id, startMs, endMs, requestedFrequencyMs)` — returns data at the requested frequency, resampling if needed
- `getRaw(id, startMs, endMs)` — returns data without resampling, for models that don't require regular spacing

`get()` resolves data in 4 steps:

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
IModel (abstract base, enable_shared_from_this)
├── IRegressionModel (virtual IModel)
│   └── BaseRegressionModel
│       └── ARModel
├── IMultivariateRegressionModel (virtual IModel)    # planned
├── IProbabilisticModel (virtual IModel)             # planned
└── IClassificationModel (virtual IModel)            # planned
```

All family interfaces use `public virtual IModel` so a model can implement multiple families (e.g., a Bayesian AR model can implement both `IRegressionModel` and `IProbabilisticModel`).

### IModel (base)

Common to all model families:
- `name()`, `print()` — identification
- `fit()` — train the model
- `isFitted()` — check training state
- `requiresRegularSpacing()`, `regularityTolerance()` — spacing constraints
- `contextSize()` — required window length

### IRegressionModel

Extends IModel for univariate regression:
- `setData(view, trainRatio, validationRatio)` — configure data splits
- `predictOneStep(window)` — single-step prediction from a context window
- `evaluate(view)` → `RegressionEvaluation` — compute regression metrics
- `createFresh()` → `unique_ptr<IRegressionModel>` — factory for re-fitting
- `getViewTimeSeriesId()` — series identifier for persistence

### Evaluation Types

- **`RegressionEvaluation`** — MSE, RMSE, MAE, R², adjusted R², log-likelihood, AIC
- **`ClassificationEvaluation`** — accuracy, precision, recall, F1, confusion matrix

### BaseRegressionModel

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
    logging::ILogger* logger_;
    ITimeSeriesSaver* saver_;
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

## Namespace Convention (Domain B)

```
namespace finance::   — pure domain types only
    Currency, IAsset, Equity, Cash, ETF, Bond
    Portfolio, Transaction, PortfolioSnapshot, SnapshotPosition
    TransactionType, AssetType, AssetId

namespace finapp::    — all infrastructure
    IAssetRepository, IPortfolioRepository, IFXRepository
    IAssetProvider, FXInfos
    AssetService, FXService, PortfolioService
    YFinanceProvider, YFinanceEquityProvider
    YahooFinanceImporter
    CSV repository implementations
```

`.cpp` implementation files inside `namespace finapp {}` blocks use `using namespace finance;` locally to avoid verbose qualification.

---

## Finance Module (Domain B)

### Currency

`enum class Currency : uint8_t` with `toString()` / `currencyFromString()` conversion. Supported: USD, EUR, JPY, KRW, CAD, GBP.

### Asset Hierarchy

```
IAsset (abstract)
├── ticker(), name(), type(), denomination()
├── priceSeriesId() — bridge to TimeSeriesService (default: ticker())
│
├── Equity — exchange, sector
├── ETF — expenseRatio, trackingIndex (planned)
├── Bond — couponRate, maturityMs, faceValue (planned)
└── Cash — denomination only, priceSeriesId() = "" (no price series)
```

`Position` struct: `{ shared_ptr<const IAsset> asset, double quantity }`

Assets are lightweight descriptors — they do NOT hold price data. Price history is fetched on demand via `AssetService::loadTimeSeriesValue()` which delegates to `TimeSeriesService`.

### Transaction

```cpp
struct Transaction {
    int64_t timestampsMs;
    TransactionType type;     // Buy, Sell, Deposit, Withdrawal, Dividend, Split
    std::string assetTicker;
    double quantity;
    double pricePerUnit;
    double fees;
    Currency SettlementCurrency;
};
```

### Portfolio

Event-sourced portfolio. Current state is derived from a `PortfolioSnapshot` + ordered transactions.

- **Builder pattern** for construction: `addPosition()`, `addCash()`, `fromSnapshot()`, `withTransactions()`
- **`apply(Transaction)`** — mutates current state (positions + cash balances)
- **`setTargetAllocation()`** / `rebalance()` — switch to target-weight mode, generate rebalancing transactions
- **`totalValue(prices, fxRates)`** / **`weights(prices, fxRates)`** — computed from current positions + market data passed by caller
- **`snapshot(timestampMs)`** — serialize current state for persistence

### PortfolioSnapshot

```cpp
struct PortfolioSnapshot {
    int64_t timestampMs;
    std::string portfolioId;
    vector<Position> positions;
    unordered_map<Currency, double> cashBalances;
};
```

---

## Finance Data Layer (Domain B)

### Repositories (namespace `finapp::`)

| Interface | Key | Purpose |
|-----------|-----|---------|
| `IAssetRepository` | `ticker` | CRUD for asset metadata (per AssetType) |
| `IPortfolioRepository` | `portfolioId` | Snapshots (point-in-time) + transaction log (append-only) + soft delete |
| `IFXRepository` | `(baseCurrency, quoteCurrency)` | Maps currency pairs to TimeSeries IDs via `FXInfos` struct |

`IAssetRepository` is one interface, but `AssetService` holds a `map<AssetType, shared_ptr<IAssetRepository>>` — different implementations per asset type since Bond metadata differs from Equity metadata.

`IPortfolioRepository::deletePortfolio()` is a soft delete. The CSV implementation renames `_snapshot.csv` → `_snapshot.csv.deleted`; `exists()` and `listPortfolioIds()` filter on `.csv` extension so deleted portfolios become invisible.

`FXInfos` struct (`finapp::`) holds `{baseCurrency, quoteCurrency, timeseriesID}`. FX series IDs follow the yfinance convention: `"<BASE><QUOTE>=X"` (e.g., `"EURUSD=X"`).

### Providers (namespace `finapp::`)

| Interface | Implementation | Purpose |
|-----------|---------------|---------|
| `IAssetProvider` | `YFinanceEquityProvider` | Fetch equity metadata (name, currency, exchange, sector) via yfinance |
| `ITimeSeriesLoader` (Domain A) | `YFinanceProvider` | Fetch OHLCV price data via yfinance; returns adjusted close |

`AssetService` holds a `map<AssetType, shared_ptr<IAssetProvider>>` mirroring the repository map.

Both `YFinanceProvider` and `YFinanceEquityProvider` use pybind11 to call `YFinanceFetcher.py` functions directly in-process via a `PythonRuntime` singleton that guards `py::initialize_interpreter()`.

### Importers (namespace `finapp::`)

**`YahooFinanceImporter`** — pure static parser, no service dependency.
- Input: Yahoo Finance portfolio CSV export path + `Config{baseCurrency, currencyResolver}`
- Output: `vector<Transaction>` sorted by timestamp (priority tiebreaker: Deposit < Dividend < Buy/Sell < Withdrawal < Split)
- `$$CASH_TX` rows → `Deposit` / `Withdrawal`; symbol rows → `Buy` / `Sell` / `Dividend`; SPLIT rows are skipped (no price in Yahoo exports)

### Services (namespace `finapp::`)

```
AssetService
├── map<AssetType, IAssetRepository>   — persist/load asset metadata
├── map<AssetType, IAssetProvider>     — fetch metadata from external sources
└── TimeSeriesService                  — price data (already cached by CachedTimeSeriesRepository)

FXService
├── IFXRepository                      — maps (EUR,USD) → timeSeriesId "EURUSD=X"
├── TimeSeriesService                  — FX rate data
└── registerPair(base, quote, id)      — explicit pair registration (optional id override)

PortfolioService
├── IPortfolioRepository               — snapshots + transactions
├── AssetService                       — resolve tickers to IAsset objects + price data
├── FXService                          — cross-currency conversion
├── createNew(id, name, currency, ts)  — seed empty portfolio + initial snapshot
└── deletePortfolio(id)                — soft delete via repository
```

### Data Flow: Load Portfolio Value Series

```
PortfolioService::valueSeries("pf1", startMs, endMs, freqMs)
    |
    v
1. Load latest snapshot from IPortfolioRepository
2. Load transactions after snapshot
3. Reconstruct Portfolio via Builder
    |
    v
4. For each position's asset:
   AssetService::loadTimeSeriesValue(assetId, startMs, endMs, freqMs)
       → TimeSeriesService::get() → CachedTimeSeriesRepository → Provider
    |
    v
5. For cross-currency positions:
   FXService::load(assetCurrency, baseCurrency, startMs, endMs, freqMs)
       → TimeSeriesService::get() → CachedTimeSeriesRepository → Provider
    |
    v
6. Walk timestamps, apply transactions at boundaries, compute value at each point
7. Return TimeSeries of portfolio value
```

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
| **Dependency Injection** | `AppContext` -> `ModelSession`, Services | Decouple from concrete implementations |
| **Decorator** | `CachedTimeSeriesRepository` wraps `ITimeSeriesRepository` | Add caching transparently |
| **Strategy** | `InterpolationStrategy` enum | Pluggable resampling algorithms |
| **Template Method** | `BaseRegressionModel` -> `ARModel` | Reuse data split logic |
| **Factory** | `IRegressionModel::createFresh()` | Create blank model copies for re-fitting |
| **Interface Segregation** | `ITimeSeriesLoader` / `ITimeSeriesSaver` | Clients depend only on what they need |
| **Builder** | `Portfolio::Builder` | Flexible portfolio construction (holdings, target-weight, snapshot restore) |
| **Event Sourcing** | `Transaction` + `PortfolioSnapshot` | Portfolio state derived from append-only transaction log |
| **Type-Dispatched Registry** | `map<AssetType, IAssetRepository>` | Per-type providers and repositories |

---

## Build Configuration

- **Standard**: C++20
- **External deps**: Eigen3 (FetchContent), GoogleTest (FetchContent)
- **Build options**: `BUILD_TESTS` (ON), `BUILD_BENCHMARKS` (OFF), `BUILD_PYTHON` (ON)

### CMake Structure

```cmake
# Root CMakeLists.txt
add_subdirectory(finlib)   # FinLib (Domain A)
add_subdirectory(finapp)   # FinApp (Domain B)
add_subdirectory(tests)    # Both domains

# finlib/CMakeLists.txt
# Defines: finlib_core, finlib_analysis, finlib_data, finlib_models, finlib_session
# All use ${CMAKE_CURRENT_SOURCE_DIR}/include for headers

# finapp/CMakeLists.txt
# Defines: finapp_providers, finapp_finance, finapp_service
# All use ${CMAKE_CURRENT_SOURCE_DIR}/include for headers
```

To extract FinLib to a separate repo later, replace `add_subdirectory(finlib)` with `FetchContent(FinLib)` — all downstream `target_link_libraries` calls remain unchanged.

---

## Frequency-Keyed Storage

Each `(seriesId, frequencyMs)` pair is independently tracked. This solves the multi-grain coverage problem:

- Daily data Jan 1 - Feb 1 is stored at key `("AAPL", 86400000)`
- Hourly data Jan 15 - Feb 1 is stored at key `("AAPL", 3600000)`
- Each key has its own `CoverageInfo`, so capabilities are never overestimated.

`TimeSeriesService` can derive coarser frequencies from finer ones via resampling, but never the reverse.
