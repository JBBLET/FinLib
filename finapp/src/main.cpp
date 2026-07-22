// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/data/providers/implementations/Yfinance/YFinanceEquityProvider.hpp"
#include "finapp/data/providers/implementations/Yfinance/YFinanceProvider.hpp"
#include "finapp/data/repository/implementation/CsvRepository/CSVPortfolioRepository.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryAssetRepository.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryFXRepository.hpp"
#include "finapp/finance/analysis/PortfolioAnalysis.hpp"
#include "finapp/finance/analysis/ReturnFeatures.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/FXService.hpp"
#include "finapp/service/PortfolioService.hpp"
#include "finapp/service/analysisService/AssetAnalysisService.hpp"
#include "finapp/service/analysisService/AssetsAnalysis/EquityAnalysisService.hpp"
#include "finapp/service/analysisService/PortfolioAnalysisService.hpp"
#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/utils/TimeUtils.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/implementation/InMemoryTimeSeriesRepository.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

using finance::AssetId;
using finance::AssetType;
using finance::Currency;
using finance::Transaction;
using finance::TransactionType;
using finance::analysis::PortfolioAnalysis;
using ts::TimeSeries;

namespace {

constexpr int64_t kDayMs = 86'400'000LL;
// Daily series → ~252 trading days a year is the conventional annualization base.
constexpr double kTradingDaysPerYear = 252.0;

int64_t ymdToMs(int year, unsigned month, unsigned day) {
    const std::chrono::sys_days sd{std::chrono::year{year} / month / day};
    return std::chrono::duration_cast<std::chrono::milliseconds>(sd.time_since_epoch()).count();
}

Transaction makeTxn(int64_t ts, TransactionType type, const std::string& ticker, double qty, double price) {
    Transaction t{};
    t.timestampsMs = ts;
    t.type = type;
    t.assetType =
        (type == TransactionType::Deposit || type == TransactionType::Withdrawal) ? AssetType::Cash : AssetType::Equity;
    t.assetTicker = ticker;
    t.quantity = qty;
    t.pricePerUnit = price;
    t.fees = 0.0;
    t.settlementCurrency = Currency::USD;
    return t;
}

// Pretty-prints mean / std-dev for a return series, both per-period (as stored)
// and annualized, so the raw vs. annualized distinction is explicit.
void reportReturnStats(const std::string& label, const ts::analysis::TimeSeriesAnalysis& a) {
    const double meanDaily = a.mean();
    const auto stdDaily = a.standardDeviation();  // sample std, std::nullopt if < 2 points

    std::cout << "  " << label << " (" << a.size() << " returns)\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "    mean   (per-period) : " << meanDaily * 100.0 << " %\n";
    if (stdDaily) std::cout << "    stddev (per-period) : " << *stdDaily * 100.0 << " %\n";

    std::cout << std::setprecision(2);
    std::cout << "    mean   (annualized) : " << meanDaily * kTradingDaysPerYear * 100.0 << " %  (x252)\n";
    if (stdDaily) {
        const double annVol = *stdDaily * std::sqrt(kTradingDaysPerYear);
        std::cout << "    vol    (annualized) : " << annVol * 100.0 << " %  (x sqrt(252))\n";
        if (annVol > 0.0) {
            const double annSharpe = (meanDaily * kTradingDaysPerYear) / annVol;
            std::cout << "    sharpe (annualized) : " << annSharpe
                      << "   = (mean/std)*sqrt(252)  <-- textbook annualized\n";
        }
    }
}

void reportScalarMetrics(const std::string& label, const std::vector<std::pair<std::string, double>>& metrics) {
    std::cout << "  " << label << " scalar metrics from the service layer:\n";
    if (metrics.empty()) {
        std::cout << "    (none)\n";
        return;
    }
    std::cout << std::fixed << std::setprecision(4);
    for (const auto& [name, value] : metrics)
        std::cout << "    " << std::setw(22) << std::left << name << value << "\n";
}

}  // namespace

int main() {
    using namespace ts::common::utils::time;

    // ---------------------------------------------------------------------
    // 1. Wire the service loop — live yfinance prices, in-memory caches,
    //    a CSV portfolio repository in a temp directory.
    // ---------------------------------------------------------------------
    auto repoDir = std::filesystem::temp_directory_path() / "finapp_returns_demo";
    std::filesystem::remove_all(repoDir);
    std::filesystem::create_directories(repoDir);

    auto innerRepo = std::make_shared<ts::InMemoryTimeSeriesRepository>();
    auto cachedRepo = std::make_shared<ts::CachedTimeSeriesRepository>(innerRepo);
    auto tsLoader = std::make_shared<finapp::YFinanceProvider>();
    auto tsService = std::make_shared<ts::TimeSeriesService>(cachedRepo, tsLoader);

    auto equityRepo = std::make_shared<finapp::InMemoryAssetRepository>();
    auto equityProvider = std::make_shared<finapp::YFinanceEquityProvider>();
    std::unordered_map<AssetType, std::shared_ptr<finapp::IAssetRepository>> repos = {{AssetType::Equity, equityRepo}};
    std::unordered_map<AssetType, std::shared_ptr<finapp::IAssetProvider>> providers = {
        {AssetType::Equity, equityProvider}};
    auto assetService = std::make_shared<finapp::AssetService>(tsService, std::move(repos), std::move(providers));

    auto fxRepo = std::make_shared<finapp::InMemoryFXRepository>();
    auto fxService = std::make_shared<finapp::FXService>(tsService, fxRepo);

    auto portfolioRepo = std::make_shared<finapp::CSVPortfolioRepository>(repoDir);
    finapp::PortfolioService portfolioService(portfolioRepo, assetService, fxService);

    // Analysis services: Equity analysis -> generic asset analysis -> portfolio analysis.
    auto equityAnalysisService = std::make_shared<finapp::EquityAnalysisService>(assetService);
    std::unordered_map<AssetType, std::shared_ptr<finapp::IAssetAnalysisService>> analysisServices = {
        {AssetType::Equity, equityAnalysisService}};
    auto assetAnalysisService =
        std::make_shared<finapp::AssetAnalysisService>(assetService, std::move(analysisServices));
    finapp::PortfolioAnalysisService portfolioAnalysisService(assetAnalysisService);

    // ---------------------------------------------------------------------
    // 2. Build a single-asset AAPL portfolio with buys and sells over 2 years.
    //    Prices below only affect the cash ledger; the NAV/return analysis
    //    uses real yfinance closes. (Today = 2026-06-26 -> window 2024..2026.)
    // ---------------------------------------------------------------------
    const std::string kId = "aapl_demo";
    const std::string kTicker = "AAPL";
    const int64_t startMs = ymdToMs(2024, 6, 26);
    const int64_t endMs = ymdToMs(2026, 6, 26);

    portfolioService.createNew(kId, "AAPL Demo", Currency::USD);

    std::vector<Transaction> txns;
    txns.push_back(makeTxn(startMs - kDayMs, TransactionType::Deposit, "USD", 60'000.0, 1.0));
    txns.push_back(makeTxn(ymdToMs(2024, 6, 27), TransactionType::Buy, kTicker, 100.0, 210.0));
    txns.push_back(makeTxn(ymdToMs(2024, 9, 16), TransactionType::Buy, kTicker, 50.0, 220.0));
    txns.push_back(makeTxn(ymdToMs(2025, 1, 10), TransactionType::Sell, kTicker, 40.0, 240.0));
    txns.push_back(makeTxn(ymdToMs(2025, 6, 20), TransactionType::Buy, kTicker, 30.0, 200.0));
    txns.push_back(makeTxn(ymdToMs(2026, 1, 15), TransactionType::Sell, kTicker, 60.0, 230.0));
    portfolioService.importTransactions(kId, std::move(txns));  // net holding: 80 AAPL

    finance::Portfolio portfolio = portfolioService.load(kId);

    std::cout << "=== Portfolio '" << kId << "' over [" << msToStringDate(startMs) << " .. " << msToStringDate(endMs)
              << "] ===\n";
    for (const auto& pos : portfolio.positions())
        std::cout << "  final position: " << pos.assetId.ticker << " x " << pos.quantity << "\n";

    // ---------------------------------------------------------------------
    // 3. NAV time series through the service loop (replays buys/sells daily).
    // ---------------------------------------------------------------------
    TimeSeries navSeries = portfolioService.valueSeries(kId, startMs, endMs, kDayMs);
    std::cout << "\nNAV series points: " << navSeries.size();
    if (navSeries.size() > 0)
        std::cout << "  first=" << navSeries.getValues().front() << "  last=" << navSeries.getValues().back();
    std::cout << "\n";

    // ---------------------------------------------------------------------
    // 4. Portfolio analysis: install the Returns Features on the replayed NAV
    //    and read the mean / std-dev / metrics the service layer computes.
    // ---------------------------------------------------------------------
    auto analysis = portfolioAnalysisService.createPortfolioAnalysis(portfolio, startMs, endMs, kDayMs);
    analysis->setNavTimeSeries(std::make_shared<const TimeSeries>(std::move(navSeries)));
    for (const auto& feature : finance::analysis::defaultReturnFeatures()) analysis->installFeature(feature);

    std::cout << "\n--- Portfolio NAV log returns ---\n";
    reportReturnStats("nav logReturn", analysis->seriesAnalysis("logReturn"));
    reportScalarMetrics("portfolio", analysis->scalarMetrics());

    // ---------------------------------------------------------------------
    // 5. Same Returns Features on the AAPL price series directly (the asset
    //    analysis already built inside the portfolio analysis).
    // ---------------------------------------------------------------------
    auto aapl = analysis->assetAnalysis(kTicker);
    aapl->installFeatures(finance::analysis::defaultReturnFeatures());

    std::cout << "\n--- AAPL price log returns ---\n";
    reportReturnStats("AAPL logReturn", aapl->derivedAnalysis("logReturn"));
    reportScalarMetrics("AAPL", aapl->scalarMetrics());

    std::cout << "\nNote: 'sharpe' from the service layer currently equals mean/std (the\n"
                 "sqrt(252) cancels in FinanceStats::sharpeRatio); compare it to the\n"
                 "annualized sharpe printed above.\n";

    std::filesystem::remove_all(repoDir);
    return 0;
}
