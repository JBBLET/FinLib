# FinLib

A C++ learning project building a financial portfolio tracking and analysis platform from scratch.

## Project Goal

Learn modern C++20 by building something real — a monorepo containing a generic time series library and a finance application built on top of it. The focus is on clean architecture, interface-driven design, and performant data handling with Eigen3.

## What It Does

Track and analyze investment portfolios: record transactions, compute portfolio value and weights over time, fetch market data, and run risk metrics — all backed by a time series engine that handles caching, resampling, and multi-frequency storage.

## Architecture

Two peer domains in a monorepo:

```
finlib/     Domain A — Generic time series library
            Works on any time-indexed data (finance, weather, sensors).
            Zero finance knowledge. Depends only on Eigen3.

finapp/     Domain B — Finance application
            Assets, portfolios, transactions, FX, risk metrics, gRPC server.
            Depends on Domain A. All external deps (Python, DB, network) live here.
```

Key design principles:
- **Caller owns all configuration** — pure dependency injection, no singletons
- **Hard domain boundary** — finlib has zero includes from finapp
- **Interfaces everywhere** — swap CSV for TimescaleDB or kdb+ by implementing one interface
- **Event sourcing** — portfolio state derived from transaction log + snapshots

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full technical breakdown.

## Roadmap

### Phase 1 — Portfolio Tracking (current)
- [x] TimeSeries core (container, views, resampling, interpolation)
- [x] Statistical analysis (mean, variance, ACF, PACF, hypothesis tests)
- [x] AR(q) model with OLS, Yule-Walker, Levinson-Durbin solvers
- [x] Data layer (CSV repository, in-memory cache, TimeSeriesService orchestration)
- [x] Online forecasting session with drift detection
- [x] Finance domain types (Currency, IAsset, Equity, Cash, Portfolio, Transaction)
- [x] Finance data interfaces (IAssetRepository, IPortfolioRepository, IFXRepository)
- [x] Service layer (AssetService, PortfolioService, FXService)
- [ ] CSV implementations for finance repositories
- [ ] Core portfolio logic (apply transactions, snapshot, value/weight computation)
- [ ] Risk metrics (Sharpe ratio, basic Greeks)
- [ ] Proto schema + gRPC PortfolioTracking server

### Phase 2 — Persistence and UI
- Replace Python scripts with pybind11 bindings
- TimescaleDB repositories (time series + finance data)
- Simple UI (TUI or Tauri)

### Phase 3 — Models and Analytics
- More regression models (ARMA, ARIMA, logistic regression)
- Classification models + feature builder
- Portfolio analytics (diversification, exposure, correlation matrix)
- Reverse Python bindings for defining models in Python

### Phase 4 — Strategy and Optimization
- IStrategy interface + backtesting engine
- Portfolio optimization (mean-variance, risk parity, Black-Litterman)
- Constraint-based allocation from asset attributes (geography, sector)

## Build

```bash
cmake -B build
cmake --build build
ctest --test-dir build --label-regex unit
```

Requires: C++20 compiler, CMake 3.14+. Eigen3 and GoogleTest are fetched automatically.

## License

Copyright (c) 2026 JBBLET. All Rights Reserved.
