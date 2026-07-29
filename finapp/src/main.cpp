// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/data/providers/implementations/Yfinance/YFinanceEquityProvider.hpp"
#include "finapp/data/providers/implementations/Yfinance/YFinanceProvider.hpp"
#include "finapp/data/repository/implementation/CsvRepository/CSVPortfolioRepository.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryAssetRepository.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryFXRepository.hpp"
#include "finapp/finance/analysis/ReturnFeatures.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/calendar/Recurrence.hpp"
#include "finapp/finance/calendar/WeekdayCalendar.hpp"
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
#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"
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
using ts::TimeSeries;

namespace {

constexpr int64_t kDayMs = 86'400'000LL;
constexpr double kTradingDaysPerYear = 252.0;

int failures = 0;

int64_t ymdToMs(int year, unsigned month, unsigned day) {
    const std::chrono::sys_days sd{std::chrono::year{year} / month / day};
    return std::chrono::duration_cast<std::chrono::milliseconds>(sd.time_since_epoch()).count();
}

Transaction makeTxn(int64_t ts, TransactionType type, const std::string& ticker, double qty, double price,
                    Currency settlement) {
    Transaction t{};
    t.timestampsMs = ts;
    t.type = type;
    t.assetType =
        (type == TransactionType::Deposit || type == TransactionType::Withdrawal) ? AssetType::Cash : AssetType::Equity;
    t.assetTicker = ticker;
    t.quantity = qty;
    t.pricePerUnit = price;
    t.fees = 0.0;
    t.settlementCurrency = settlement;
    return t;
}

std::string label(const AssetId& assetId) { return finance::assetTypeToString(assetId.type) + ":" + assetId.ticker; }

// Each check prints its own verdict so the run doubles as a report.
void check(const std::string& what, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "  [ OK ]   " : "  [FAIL]   ") << what;
    if (!detail.empty()) std::cout << "  (" << detail << ")";
    std::cout << "\n";
    if (!ok) ++failures;
}

void checkNear(const std::string& what, double actual, double expected, double tol) {
    const double diff = std::abs(actual - expected);
    std::ostringstream detail;
    detail << std::fixed << std::setprecision(6) << "actual=" << actual << " expected=" << expected << " diff=" << diff;
    check(what, diff <= tol, detail.str());
}

void reportReturnStats(const std::string& lbl, const ts::analysis::TimeSeriesAnalysis& a) {
    const double meanDaily = a.mean();
    const auto stdDaily = a.standardDeviation();
    std::cout << "  " << lbl << " (" << a.size() << " returns)\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "    mean (annualized) : " << meanDaily * kTradingDaysPerYear * 100.0 << " %\n";
    if (stdDaily)
        std::cout << "    vol  (annualized) : " << *stdDaily * std::sqrt(kTradingDaysPerYear) * 100.0 << " %\n";
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

    auto equityAnalysisService = std::make_shared<finapp::EquityAnalysisService>(assetService);
    std::unordered_map<AssetType, std::shared_ptr<finapp::IAssetAnalysisService>> analysisServices = {
        {AssetType::Equity, equityAnalysisService}};
    auto assetAnalysisService =
        std::make_shared<finapp::AssetAnalysisService>(assetService, std::move(analysisServices));
    finapp::PortfolioAnalysisService portfolioAnalysisService(assetAnalysisService);

    // ---------------------------------------------------------------------
    // 2. A multi-asset, multi-currency portfolio. AIR.PA is EUR-denominated in
    //    a USD-base portfolio, so the orchestrator must fold FX into the price
    //    (priceInBase = price x fx) and convert the EUR cash leg too. The buys
    //    and sells produce several snapshots, so the value/weight series are
    //    stitched from several segments rather than a single one.
    // ---------------------------------------------------------------------
    const std::string kId = "multi_demo";
    const Currency kBase = Currency::USD;
    const int64_t startMs = ymdToMs(2024, 7, 1);
    const int64_t endMs = ymdToMs(2026, 7, 1);

    const AssetId kAapl{AssetType::Equity, "AAPL"};   // USD
    const AssetId kMsft{AssetType::Equity, "MSFT"};   // USD
    const AssetId kAir{AssetType::Equity, "AIR.PA"};  // EUR
    const AssetId kCashUsd{AssetType::Cash, finance::toString(Currency::USD)};
    const AssetId kCashEur{AssetType::Cash, finance::toString(Currency::EUR)};

    portfolioService.createNew(kId, "Multi-Currency Demo", Currency::USD);

    std::vector<Transaction> txns;
    // Funded the day *before* the grid starts — the covering snapshot is a predecessor,
    // which is the case that must resolve at tick 0.
    txns.push_back(makeTxn(startMs - kDayMs, TransactionType::Deposit, "USD", 150'000.0, 1.0, Currency::USD));
    txns.push_back(makeTxn(startMs - kDayMs, TransactionType::Deposit, "EUR", 50'000.0, 1.0, Currency::EUR));
    txns.push_back(makeTxn(ymdToMs(2024, 7, 8), TransactionType::Buy, "AAPL", 100.0, 210.0, Currency::USD));
    txns.push_back(makeTxn(ymdToMs(2024, 9, 16), TransactionType::Buy, "MSFT", 50.0, 440.0, Currency::USD));
    txns.push_back(makeTxn(ymdToMs(2024, 11, 12), TransactionType::Buy, "AIR.PA", 200.0, 130.0, Currency::EUR));
    txns.push_back(makeTxn(ymdToMs(2025, 3, 10), TransactionType::Sell, "AAPL", 40.0, 240.0, Currency::USD));
    txns.push_back(makeTxn(ymdToMs(2025, 9, 22), TransactionType::Buy, "AAPL", 30.0, 200.0, Currency::USD));
    txns.push_back(makeTxn(ymdToMs(2026, 1, 15), TransactionType::Sell, "MSFT", 10.0, 500.0, Currency::USD));
    portfolioService.importTransactions(kId, std::move(txns));

    finance::Portfolio portfolio = portfolioService.load(kId);

    std::cout << "=== Portfolio '" << kId << "' over [" << msToStringDate(startMs) << " .. " << msToStringDate(endMs)
              << "] ===\n";
    for (const auto& pos : portfolio.positions())
        std::cout << "  final position: " << pos.assetId.ticker << " x " << pos.quantity << "\n";
    for (const auto& [ccy, amount] : portfolio.cashBalances())
        std::cout << "  final cash:     " << finance::toString(ccy) << " " << std::fixed << std::setprecision(2)
                  << amount << "\n";

    // Confirm the FX path is genuinely engaged rather than collapsing to identity.
    const Currency airDenom = assetService->load(kAir)->denomination();
    std::cout << "\n  AIR.PA denomination: " << finance::toString(airDenom) << "  (base is "
              << finance::toString(Currency::USD) << ")\n";

    // ---------------------------------------------------------------------
    // 3. The new orchestrator API: total + weights in a single pass.
    // ---------------------------------------------------------------------
    std::cout << "\n--- PortfolioService::valueAndWeightSeries ---\n";
    finance::PortfolioSeries series = portfolioService.valueAndWeightSeries(kId, startMs, endMs, kDayMs);

    const auto& total = series.total.getValues();
    std::cout << "  total series points: " << series.total.size() << "  first=" << std::fixed << std::setprecision(2)
              << total.front() << "  last=" << total.back() << "\n";
    std::cout << "  weight series returned: " << series.weights.size() << "\n";

    check("total series is non-empty", series.total.size() > 0);
    // The deposits land the day before the grid starts, so tick 0 is covered by a
    // predecessor snapshot. A zero here is the mask-at-grid-start bug.
    check("tick 0 is funded (predecessor snapshot resolves)",
          total.front() > 0.0,
          "first=" + std::to_string(total.front()));
    check("every tick is funded", std::ranges::all_of(total, [](double v) { return v > 0.0; }));

    // ---------------------------------------------------------------------
    // 4. Weights must sum to 1 at every tick — the core invariant of the
    //    mask-and-sum stitching across segments.
    // ---------------------------------------------------------------------
    std::cout << "\n--- weight invariants ---\n";
    double worstWeightSumDeviation = 0.0;
    for (size_t i = 0; i < series.total.size(); ++i) {
        double sum = 0.0;
        for (const auto& [assetId, w] : series.weights) sum += w.getValues()[i];
        worstWeightSumDeviation = std::max(worstWeightSumDeviation, std::abs(sum - 1.0));
    }
    checkNear("weights sum to 1.0 at every tick (worst case shown)", 1.0 + worstWeightSumDeviation, 1.0, 1e-9);

    std::cout << "\n  weights at " << msToStringDate(endMs) << ":\n";
    std::cout << std::fixed << std::setprecision(4);
    for (const auto& [assetId, w] : series.weights)
        std::cout << "    " << std::setw(16) << std::left << label(assetId) << w.getValues().back() * 100.0 << " %\n";

    // ---------------------------------------------------------------------
    // 5. The narrower APIs must agree with the combined one. valueSeries and
    //    weightsSeries run their own fetch/segment loops, so this cross-checks
    //    those against valueAndWeightSeries.
    // ---------------------------------------------------------------------
    std::cout << "\n--- cross-checks between service APIs ---\n";
    TimeSeries valueOnly = portfolioService.valueSeries(kId, startMs, endMs, kDayMs);
    double worstValueDiff = 0.0;
    if (valueOnly.size() == series.total.size()) {
        for (size_t i = 0; i < valueOnly.size(); ++i)
            worstValueDiff = std::max(worstValueDiff, std::abs(valueOnly.getValues()[i] - total[i]));
        checkNear("valueSeries matches valueAndWeightSeries.total at every tick", worstValueDiff, 0.0, 1e-6);
    } else {
        check("valueSeries has the same length as valueAndWeightSeries.total", false);
    }

    auto weightsOnly = portfolioService.weightsSeries(kId, startMs, endMs, kDayMs);
    check("weightsSeries returns the same assets as valueAndWeightSeries.weights",
          weightsOnly.size() == series.weights.size(),
          std::to_string(weightsOnly.size()) + " vs " + std::to_string(series.weights.size()));

    // computeOverviewAtTs still runs the older scalar price/FX path. Note this compares
    // more than the arithmetic: the series path fetches prices via TimeSeriesService::get
    // (InterpolationStrategy::Nearest, which may snap forward to a later bar) while the
    // scalar path uses getSinglePoint (exact, else latestValue — look-back only). Until
    // those two agree on a strategy, the totals differ by roughly one bar of drift.
    auto overview = portfolioService.computeOverviewAtTs(kId, endMs);
    checkNear("computeOverviewAtTs total agrees with the series (Nearest vs latestValue fetch)",
              overview.totalValue,
              total.back(),
              std::max(1.0, total.back() * 1e-6));

    // ---------------------------------------------------------------------
    // 6. quantitySeries is now public — its final tick must match the replayed
    //    portfolio's positions and cash.
    // ---------------------------------------------------------------------
    std::cout << "\n--- quantitySeries ---\n";
    auto quantities = portfolioService.quantitySeries(kId, startMs, endMs, kDayMs);
    for (const auto& [assetId, q] : quantities)
        std::cout << "    " << std::setw(16) << std::left << label(assetId) << std::setprecision(2)
                  << q.getValues().back() << "\n";

    for (const auto& pos : portfolio.positions()) {
        auto it = quantities.find(pos.assetId);
        if (it == quantities.end()) {
            check("quantitySeries covers " + label(pos.assetId), false);
            continue;
        }
        checkNear("quantitySeries final tick matches position " + pos.assetId.ticker,
                  it->second.getValues().back(),
                  pos.quantity,
                  1e-9);
    }
    for (const auto& cashId : {kCashUsd, kCashEur})
        check("quantitySeries covers " + label(cashId), quantities.contains(cashId));

    // ---------------------------------------------------------------------
    // 7. The NAV from the new API feeds the existing analysis stack unchanged.
    // ---------------------------------------------------------------------
    std::cout << "\n--- Portfolio NAV log returns (NAV from valueAndWeightSeries) ---\n";
    auto analysis = portfolioAnalysisService.createPortfolioAnalysis(portfolio, startMs, endMs, kDayMs);
    analysis->setNavTimeSeries(std::make_shared<const TimeSeries>(series.total));
    for (const auto& feature : finance::analysis::defaultReturnFeatures()) analysis->installFeature(feature);
    reportReturnStats("nav logReturn", analysis->seriesAnalysis("logReturn"));

    std::filesystem::remove_all(repoDir);

    std::cout << "\n=== " << (failures == 0 ? "ALL CHECKS PASSED" : std::to_string(failures) + " CHECK(S) FAILED")
              << " ===\n";

    // ---------------------------------------------------------------------
    // Monte Carlo Simulation
    // ---------------------------------------------------------------------

    // Constants
    const int64_t simStart = ymdToMs(2026, 7, 2);
    const int64_t simEnd = ymdToMs(2041, 7, 31);
    const double monthlyContribution = 1000.00;
    const int numPaths = 10'000;
    const double initialInvest = 10'000.00;
    finance::WeekdayCalendar cal;

    // Series Starting
    auto histGrid = cal.schedule(startMs, simStart);
    TimeSeries aaplHist = assetService->loadTimeSeriesValue(kAapl, histGrid);
    const auto& hv = aaplHist.getValues();
    const double spot = hv.back();

    std::vector<double> r;
    r.reserve(hv.size());
    for (size_t i = 1; i < hv.size(); ++i)
        if (hv[i - 1] > 0.0 && hv[i] > 0.0) r.push_back(std::log(hv[i] / hv[i - 1]));
    const double meanDaily = std::accumulate(r.begin(), r.end(), 0.0) / r.size();
    double var = 0.0;
    for (double x : r) var += (x - meanDaily) * (x - meanDaily);
    var /= (r.size() - 1);
    const double stdDaily = std::sqrt(var);

    // Constribution calendar
    auto gridPtr = cal.schedule(simStart, simEnd);
    const auto& grid = *gridPtr;
    auto contribDates =
        finance::Recurrence{.start = grid.front(), .end = simEnd, .frequency = finance::Frequency::Monthly}.generate(
            cal);

    auto runPath = [&](std::mt19937_64& rng) -> double {
        std::normal_distribution<double> z(0.0, 1.0);
        finance::Portfolio pf = finance::Portfolio::Builder("mc", "mc", kBase).build();
        double price = spot;
        size_t ci = 0, di = 0;
        pf.apply(makeTxn(grid.front(), TransactionType::Deposit, "USD", initialInvest, 1.0, kBase));
        pf.apply(makeTxn(grid.front(), TransactionType::Buy, kAapl.ticker, initialInvest / price, price, kBase));
        while (ci < contribDates.size() && contribDates[ci] == grid.front()) ++ci;

        for (size_t t = 1; t < grid.size(); ++t) {
            const int64_t ts = grid[t];
            price *= std::exp(meanDaily + stdDaily * z(rng));
            while (ci < contribDates.size() && contribDates[ci] == ts) {
                ++ci;
                pf.apply(makeTxn(ts, TransactionType::Deposit, toString(kBase), monthlyContribution, 1.0, kBase));
                const double cash = pf.cashBalance(kBase);
                if (cash > 0.0 && price > 0.0)
                    pf.apply(makeTxn(ts, TransactionType::Buy, kAapl.ticker, cash / price, price, kBase));
            }
        }
        return pf.valuation({{kAapl, price}}, {{kBase, 1.0}}).total;
    };
    std::vector<double> terminal(numPaths);
    std::mt19937_64 rng(0xC0FFEE);
    for (int p = 0; p < numPaths; ++p) terminal[p] = runPath(rng);
    std::sort(terminal.begin(), terminal.end());

    auto pct = [&](double q) { return terminal[std::min<size_t>(q * numPaths, numPaths - 1)]; };
    const double mean = std::accumulate(terminal.begin(), terminal.end(), 0.0) / numPaths;
    const double invested = initialInvest + monthlyContribution * static_cast<double>(contribDates.size() - 1);

    std::cout << std::fixed << std::setprecision(0);
    std::cout << "\n=== Monte Carlo (" << numPaths << " paths, " << grid.size() << " steps) ===\n";
    std::cout << "  daily log-return: m=" << std::setprecision(5) << meanDaily << " s=" << stdDaily << "\n"
              << std::setprecision(0);
    std::cout << "  annualized log-return: m=" << std::setprecision(5) << meanDaily * 252
              << " s=" << stdDaily * std::sqrt(252) << "\n"
              << std::setprecision(0);
    std::cout << "  invested total : " << invested << "\n";
    std::cout << "  mean terminal  : " << mean << "\n";
    std::cout << "  p5  / p50 / p95: " << pct(0.05) << " / " << pct(0.50) << " / " << pct(0.95) << "\n";
}
