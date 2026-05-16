// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>

#include <memory>

#include "finapp/service/PortfolioService.hpp"
#include "grpcpp/server.h"
#include "grpcpp/server_context.h"
#include "portfolio.grpc.pb.h"
#include "portfolio.pb.h"

class PortfolioGrpcServiceImpl final : public finapp_rpc::PortfolioService::Service {
 public:
    explicit PortfolioGrpcServiceImpl(std::shared_ptr<finapp::PortfolioService> portfolioService)
        : portfolioService_{std::move(portfolioService)} {}

    // Portfolio Management
    grpc::Status ListPortfoliosSummary(grpc::ServerContext*, const finapp_rpc::ListPortfoliosSummaryInput* request,
                                       finapp_rpc::ListPortfoliosSummaryOutput* reply) override;
    grpc::Status GetPortfoliosByIds(grpc::ServerContext*, const finapp_rpc::GetPortfoliosByIdsInput* request,
                                    finapp_rpc::GetPortfoliosByIdsOutput* reply) override;

    grpc::Status CreatePortfolio(grpc::ServerContext*, const finapp_rpc::CreatePortfolioInput* request,
                                 finapp_rpc::CreatePortfolioOutput* reply) override;

    grpc::Status DeletePortfolioById(grpc::ServerContext*, const finapp_rpc::DeletePortfolioByIdInput* request,
                                     finapp_rpc::DeletePortfolioByIdOutput* reply) override;

    // Portfolio Tracking
    grpc::Status GetPortfolioTimeSeriesById(grpc::ServerContext*,
                                            const finapp_rpc::GetPortfolioTimeSeriesByIdInput* request,
                                            finapp_rpc::GetPortfolioTimeSeriesByIdOutput* reply) override;

    // Portfolio analysis
    grpc::Status GetPortfolioAnalysisById(grpc::ServerContext*,
                                          const finapp_rpc::GetPortfolioAnalysisByIdInput* request,
                                          finapp_rpc::GetPortfolioAnalysisByIdOutput* reply) override;

    // Transaction Management
    grpc::Status ListPortfolioTransactionsByPortfolioId(
        grpc::ServerContext*, const finapp_rpc::ListPortfolioTransactionsByPortfolioIdInput* request,
        finapp_rpc::ListPortfolioTransactionsByPortfolioIdOutput* reply) override;

    grpc::Status RequestAddTransaction(grpc::ServerContext*, const finapp_rpc::RequestAddTransactionInput* request,
                                       finapp_rpc::RequestAddTransactionOutput* reply) override;

 private:
    std::shared_ptr<finapp::PortfolioService> portfolioService_;
};
