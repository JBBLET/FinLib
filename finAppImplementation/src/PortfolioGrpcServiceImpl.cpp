// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "grpcServiceImplementation/PortfolioGrpcServiceImpl.hpp"

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>

#include "converters/ProtoConverters.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "grpcpp/server.h"
#include "grpcpp/server_context.h"
#include "portfolio.pb.h"

// ===================================
// Portfolio Management
// ===================================
grpc::Status PortfolioGrpcServiceImpl::ListPortfoliosSummary(grpc::ServerContext*,
                                                             const finapp_rpc::ListPortfoliosSummaryInput* request,
                                                             finapp_rpc::ListPortfoliosSummaryOutput* reply) {
    try {
        for (const auto& id : portfolioService_->listPortfolioIds()) {
            auto* ident = reply->add_listportfoliosidentification();
            ident->set_id(id);
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
    }
}

grpc::Status PortfolioGrpcServiceImpl::GetPortfoliosByIds(grpc::ServerContext*,
                                                          const finapp_rpc::GetPortfoliosByIdsInput* request,
                                                          finapp_rpc::GetPortfoliosByIdsOutput* reply) {
    try {
        const int64_t ts = request->timestampms();
        for (const auto& id : request->id()) {
            finance::Portfolio portfolio = portfolioService_->load(id);
            const double total = portfolioService_->totalValue(portfolio, ts);
            const auto weights = portfolioService_->weights(portfolio, ts);
            *reply->add_listportfolios() = finapp_rpc::converters::toProto(portfolio, total, weights);
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
    }
}

grpc::Status PortfolioGrpcServiceImpl::CreatePortfolio(grpc::ServerContext*,
                                                       const finapp_rpc::CreatePortfolioInput* request,
                                                       finapp_rpc::CreatePortfolioOutput* reply) {
    try {
        const std::string id = std::to_string(request->timestampms()) + "_" + request->name();
        finance::Currency base = finapp_rpc::converters::fromProto(request->basecurrency());
        portfolioService_->createNew(id, request->name(), base, request->timestampms());
        reply->set_id(id);
        return grpc::Status::OK;
    } catch (std::exception& e) {
        return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
    }
}

grpc::Status PortfolioGrpcServiceImpl::DeletePortfolioById(grpc::ServerContext*,
                                                           const finapp_rpc::DeletePortfolioByIdInput* request,
                                                           finapp_rpc::DeletePortfolioByIdOutput* reply) {
    try {
        const std::string id = request->id();
        portfolioService_->deletePortfolio(id);
        reply->set_id(id);
        return grpc::Status::OK;
    } catch (std::exception& e) {
        return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
    }
}
// ===================================
// Portfolio Tracking
// ===================================
grpc::Status PortfolioGrpcServiceImpl::GetPortfolioTimeSeriesById(
    grpc::ServerContext*, const finapp_rpc::GetPortfolioTimeSeriesByIdInput* request,
    finapp_rpc::GetPortfolioTimeSeriesByIdOutput* reply) {
    try {
        const std::string& id = request->id();
        const int64_t startMs = request->startms();
        const int64_t endMs = request->endms();
        const int64_t deltaMs = request->deltatms() > 0 ? request->deltatms() : 86'400'000;
        finance::Portfolio portfolio = portfolioService_->load(id);

        const double total = portfolioService_->totalValue(portfolio, endMs);
        const auto wts = portfolioService_->weights(portfolio, endMs);
        *reply->mutable_portfolio() = finapp_rpc::converters::toProto(portfolio, total, wts);

        const TimeSeries vs = portfolioService_->valueSeries(id, startMs, endMs, deltaMs);
        auto* ts = reply->mutable_timeseries();
        for (int64_t t : vs.getTimestamps()) ts->add_timestampsms(t);
        for (double v : vs.getValues()) ts->add_closevalues(v);

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
    }
}

// ===================================
// Portfolio analysis
// ===================================
grpc::Status PortfolioGrpcServiceImpl::GetPortfolioAnalysisById(
    grpc::ServerContext*, const finapp_rpc::GetPortfolioAnalysisByIdInput* request,
    finapp_rpc::GetPortfolioAnalysisByIdOutput* reply) {
    try {
        const int64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        finance::Portfolio portfolio = portfolioService_->load(request->id());
        const double total = portfolioService_->totalValue(portfolio, nowMs);
        const auto weights = portfolioService_->weights(portfolio, nowMs);
        *reply->mutable_portfolio() = finapp_rpc::converters::toProto(portfolio, total, weights);
        reply->mutable_analysis()->set_emptyreturn("Analysis not yet implemented");
        return grpc::Status::OK;
    } catch (std::exception& e) {
        return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
    }
}
// ===================================
// Transaction Management
// ===================================
grpc::Status PortfolioGrpcServiceImpl::ListPortfolioTransactionsByPortfolioId(
    grpc::ServerContext*, const finapp_rpc::ListPortfolioTransactionsByPortfolioIdInput* request,
    finapp_rpc::ListPortfolioTransactionsByPortfolioIdOutput* reply) {
    try {
        const auto transactionList = portfolioService_->listTransactions(request->portfolioid());
        for (const auto& transaction : transactionList) {
            *reply->add_transactionlist() = finapp_rpc::converters::toProto(transaction);
        }
        return grpc::Status::OK;
    } catch (std::exception& e) {
        return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
    }
}

grpc::Status PortfolioGrpcServiceImpl::RequestAddTransaction(grpc::ServerContext*,
                                                             const finapp_rpc::RequestAddTransactionInput* request,
                                                             finapp_rpc::RequestAddTransactionOutput* reply) {
    try {
        const int64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        finance::Transaction transaction = finapp_rpc::converters::fromProto(request->transaction());
        const std::string transactionId = portfolioService_->addTransaction(request->portfolioid(), transaction, nowMs);
        reply->set_transactionid(transactionId);
        return grpc::Status::OK;
    } catch (std::exception& e) {
        return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
    }
}
