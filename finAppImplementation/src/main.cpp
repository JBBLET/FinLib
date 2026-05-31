// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <grpcpp/grpcpp.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
#include "finapp/common/logger/ConsoleLogger.hpp"
#include "finlib/common/logger/ConsoleLogger.hpp"
#include "finlib/data/implementation/CSVRepository.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "grpcServiceImplementation/LoggingInterceptor.hpp"
#include "grpcServiceImplementation/PortfolioGrpcServiceImpl.hpp"

namespace fs = std::filesystem;

int main() {
    auto logger = std::make_shared<finapp::logging::ConsoleLogger>();   // finapp + gRPC layer
    auto finlibLogger = std::make_shared<logging::ConsoleLogger>();      // finlib data stack

    // Paths
    const fs::path dataDir = "/tmp/finapp_test_data";
    const fs::path venvPython = "/home/jbblet/user/Documents/Projects/FinLib/.venv/bin/python3";
    const fs::path yfinanceScript = "/home/jbblet/user/Documents/Projects/FinLib/finapp/scripts/YFinanceFetcher.py";

    fs::create_directories(dataDir / "timeseries");
    fs::create_directories(dataDir / "assets");
    fs::create_directories(dataDir / "fx");
    fs::create_directories(dataDir / "portfolios");

    // TimeSeriesService: CSV cache + YFinance as the network loader
    auto csvTsRepo = std::make_shared<CSVRepository>(dataDir / "timeseries", finlibLogger.get());
    auto cachedRepo = std::make_shared<CachedTimeSeriesRepository>(csvTsRepo, finlibLogger.get());
    auto yfinance = std::make_shared<finapp::YFinanceProvider>(venvPython.string(), yfinanceScript.string());
    auto tsSvc = std::make_shared<TimeSeriesService>(cachedRepo, yfinance, finlibLogger.get());

    // AssetService
    std::unordered_map<finance::AssetType, std::shared_ptr<finapp::IAssetRepository>> assetRepos{
        {finance::AssetType::Equity, std::make_shared<finapp::CSVEquityRepository>(dataDir / "assets")},
        {finance::AssetType::Cash, std::make_shared<finapp::CSVCashRepository>(dataDir / "assets")},
    };
    std::unordered_map<finance::AssetType, std::shared_ptr<finapp::IAssetProvider>> assetProviders{
        {finance::AssetType::Equity, std::make_shared<finapp::YFinanceEquityProvider>()},
    };
    auto assetSvc =
        std::make_shared<finapp::AssetService>(tsSvc, std::move(assetRepos), std::move(assetProviders), logger.get());

    // FXService
    auto fxRepo = std::make_shared<finapp::CSVFXRepository>(dataDir / "fx");
    auto fxSvc = std::make_shared<finapp::FXService>(tsSvc, fxRepo, logger.get());

    // PortfolioService
    auto portfolioRepo = std::make_shared<finapp::CSVPortfolioRepository>(dataDir / "portfolios");
    auto portfolioSvc = std::make_shared<finapp::PortfolioService>(portfolioRepo, assetSvc, fxSvc, logger.get());

    // gRPC server
    const std::string address = "0.0.0.0:50051";
    PortfolioGrpcServiceImpl handler{portfolioSvc, logger.get()};

    std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>> interceptors;
    interceptors.push_back(std::make_unique<LoggingInterceptorFactory>(logger.get()));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&handler);
    builder.experimental().SetInterceptorCreators(std::move(interceptors));

    auto server = builder.BuildAndStart();
    logger->write(finapp::logging::Level::Info, "Listening on " + address);
    server->Wait();
}
