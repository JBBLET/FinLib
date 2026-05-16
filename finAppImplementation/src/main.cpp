// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <grpcpp/grpcpp.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "finapp/data/providers/implementations/Yfinance/YFinanceEquityProvider.hpp"
#include "finapp/data/providers/implementations/Yfinance/YFinanceProvider.hpp"
#include "finapp/data/repository/implementation/CsvRepository/CSVCashRepository.hpp"
#include "finapp/data/repository/implementation/CsvRepository/CSVEquityRepository.hpp"
#include "finapp/data/repository/implementation/CsvRepository/CSVFXRepository.hpp"
#include "finapp/data/repository/implementation/CsvRepository/CSVPortfolioRepository.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/FXService.hpp"
#include "finapp/service/PortfolioService.hpp"
#include "finlib/data/implementation/CSVRepository.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "grpcServiceImplementation/PortfolioGrpcServiceImpl.hpp"

namespace fs = std::filesystem;
using namespace finapp;
using namespace finance;

int main() {
    // -------------------------------------------------------------------------
    // Paths
    // -------------------------------------------------------------------------
    const fs::path dataDir        = "/tmp/finapp_test_data";
    const fs::path venvPython     = "/home/jbblet/user/Documents/Projects/FinLib/.venv/bin/python3";
    const fs::path yfinanceScript = "/home/jbblet/user/Documents/Projects/FinLib/finapp/scripts/YFinanceFetcher.py";

    fs::create_directories(dataDir / "timeseries");
    fs::create_directories(dataDir / "assets");
    fs::create_directories(dataDir / "fx");
    fs::create_directories(dataDir / "portfolios");

    // -------------------------------------------------------------------------
    // TimeSeriesService: CSV cache + YFinance as the network loader
    // -------------------------------------------------------------------------
    auto csvTsRepo  = std::make_shared<CSVRepository>(dataDir / "timeseries");
    auto cachedRepo = std::make_shared<CachedTimeSeriesRepository>(csvTsRepo);
    auto yfinance   = std::make_shared<YFinanceProvider>(venvPython.string(), yfinanceScript.string());
    auto tsSvc      = std::make_shared<TimeSeriesService>(cachedRepo, yfinance);

    // -------------------------------------------------------------------------
    // AssetService
    // -------------------------------------------------------------------------
    std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> assetRepos{
        {AssetType::Equity, std::make_shared<CSVEquityRepository>(dataDir / "assets")},
        {AssetType::Cash,   std::make_shared<CSVCashRepository>(dataDir / "assets")},
    };
    std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>> assetProviders{
        {AssetType::Equity, std::make_shared<YFinanceEquityProvider>()},
    };
    auto assetSvc = std::make_shared<AssetService>(tsSvc, std::move(assetRepos), std::move(assetProviders));

    // -------------------------------------------------------------------------
    // FXService
    // -------------------------------------------------------------------------
    auto fxRepo = std::make_shared<CSVFXRepository>(dataDir / "fx");
    auto fxSvc  = std::make_shared<FXService>(tsSvc, fxRepo);

    // -------------------------------------------------------------------------
    // PortfolioService
    // -------------------------------------------------------------------------
    auto portfolioRepo = std::make_shared<CSVPortfolioRepository>(dataDir / "portfolios");
    auto portfolioSvc  = std::make_shared<PortfolioService>(portfolioRepo, assetSvc, fxSvc);

    // -------------------------------------------------------------------------
    // gRPC server
    // -------------------------------------------------------------------------
    const std::string address = "0.0.0.0:50051";
    PortfolioGrpcServiceImpl handler{portfolioSvc};

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&handler);

    auto server = builder.BuildAndStart();
    std::cout << "Listening on " << address << "\n";
    server->Wait();
}
