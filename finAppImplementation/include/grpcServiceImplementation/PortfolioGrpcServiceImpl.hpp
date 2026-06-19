// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "finapp/common/logger/ILogger.hpp"
#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/service/PortfolioService.hpp"
#include "finapp/service/analysisService/PortfolioAnalysisService.hpp"
#include "grpcpp/server.h"
#include "grpcpp/server_context.h"
#include "portfolio.grpc.pb.h"
#include "portfolio.pb.h"

class PortfolioGrpcServiceImpl final : public finapp_rpc::PortfolioService::Service {
 public:
    PortfolioGrpcServiceImpl(std::shared_ptr<finapp::PortfolioService> portfolioService,
                             std::shared_ptr<finapp::PortfolioAnalysisService> portfolioAnalysisService,
                             finapp::logging::ILogger* logger = nullptr);

    // --- Portfolio CRUD ---
    grpc::Status ListPortfoliosSummary(grpc::ServerContext*, const finapp_rpc::ListPortfoliosSummaryInput*,
                                       finapp_rpc::ListPortfoliosSummaryOutput*) override;
    grpc::Status GetPortfoliosByIds(grpc::ServerContext*, const finapp_rpc::GetPortfoliosByIdsInput*,
                                    finapp_rpc::GetPortfoliosByIdsOutput*) override;
    grpc::Status CreatePortfolio(grpc::ServerContext*, const finapp_rpc::CreatePortfolioInput*,
                                 finapp_rpc::CreatePortfolioOutput*) override;
    grpc::Status DeletePortfolioById(grpc::ServerContext*, const finapp_rpc::DeletePortfolioByIdInput*,
                                     finapp_rpc::DeletePortfolioByIdOutput*) override;

    // --- NAV time series ---
    grpc::Status GetPortfolioTimeSeriesById(grpc::ServerContext*, const finapp_rpc::GetPortfolioTimeSeriesByIdInput*,
                                            finapp_rpc::GetPortfolioTimeSeriesByIdOutput*) override;

    // --- Stateless analysis ---
    grpc::Status ComputePortfolioAnalysis(grpc::ServerContext*, const finapp_rpc::ComputePortfolioAnalysisInput*,
                                          finapp_rpc::ComputePortfolioAnalysisOutput*) override;

    // --- Stateful analysis session ---
    grpc::Status OpenPortfolioAnalysisSession(grpc::ServerContext*,
                                              const finapp_rpc::OpenPortfolioAnalysisSessionInput*,
                                              finapp_rpc::OpenPortfolioAnalysisSessionOutput*) override;
    grpc::Status UpdatePortfolioAnalysisSessionRange(grpc::ServerContext*, const finapp_rpc::UpdateSessionRangeInput*,
                                                     finapp_rpc::UpdatePortfolioAnalysisSessionOutput*) override;
    grpc::Status UpdatePortfolioAnalysisSessionTimestamp(grpc::ServerContext*,
                                                         const finapp_rpc::UpdatePortfolioSessionTimestampInput*,
                                                         finapp_rpc::UpdatePortfolioSessionTimestampOutput*) override;
    grpc::Status ClosePortfolioAnalysisSession(grpc::ServerContext*, const finapp_rpc::CloseSessionInput*,
                                               finapp_rpc::CloseSessionOutput*) override;

    // --- Asset session extracted from portfolio session ---
    grpc::Status ExtractAssetAnalysisFromPortfolioSession(grpc::ServerContext*,
                                                          const finapp_rpc::ExtractAssetFromPortfolioSessionInput*,
                                                          finapp_rpc::ExtractAssetFromPortfolioSessionOutput*) override;
    grpc::Status CloseAssetSession(grpc::ServerContext*, const finapp_rpc::CloseSessionInput*,
                                   finapp_rpc::CloseSessionOutput*) override;

    // --- Transaction management ---
    grpc::Status ListPortfolioTransactionsByPortfolioId(
        grpc::ServerContext*, const finapp_rpc::ListPortfolioTransactionsByPortfolioIdInput*,
        finapp_rpc::ListPortfolioTransactionsByPortfolioIdOutput*) override;
    grpc::Status RequestAddTransaction(grpc::ServerContext*, const finapp_rpc::RequestAddTransactionInput*,
                                       finapp_rpc::RequestAddTransactionOutput*) override;
    grpc::Status RequestAddTransactionByCsv(grpc::ServerContext*, const finapp_rpc::RequestAddTransactionByCsvInput*,
                                            finapp_rpc::RequestAddTransactionOutput*) override;
    grpc::Status DeleteTransaction(grpc::ServerContext*, const finapp_rpc::DeleteTransactionInput*,
                                   finapp_rpc::DeleteTransactionOutput*) override;

 private:
    std::shared_ptr<finapp::PortfolioService> portfolioService_;
    std::shared_ptr<finapp::PortfolioAnalysisService> portfolioAnalysisService_;
    std::unique_ptr<finapp::logging::ILogger> logger_;

    struct SessionEntry {
        std::string portfolioId;
        finance::Portfolio portfolio;
        std::shared_ptr<finance::analysis::PortfolioAnalysis> analysis;
    };
    std::unordered_map<std::string, SessionEntry> sessions_;

    struct AssetSessionEntry {
        std::shared_ptr<finance::analysis::IAssetAnalysis> analysis;
    };
    std::unordered_map<std::string, AssetSessionEntry> assetSessions_;

    static std::string makeHandle_(const char* prefix);
};
