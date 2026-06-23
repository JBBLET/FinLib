// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "grpcServiceImplementation/PortfolioGrpcServiceImpl.hpp"

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>

#include "converters/ProtoConverters.hpp"
#include "finapp/common/Exception.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finapp/data/importers/YahooFinanceImporter.hpp"
#include "finapp/finance/analysis/PortfolioAnalysis.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "grpcpp/server.h"
#include "grpcpp/server_context.h"
#include "portfolio.pb.h"

// Log the error via the service's logger (if set) and return an INTERNAL gRPC status.
#define GRPC_LOG_AND_RETURN_INTERNAL(e)                                                                         \
    do {                                                                                                        \
        std::string trace_;                                                                                     \
        if (const auto* t = std::dynamic_cast<const finapp::Traced>(&(e))) trace_ = "\n" + t->formattedTrace(); \
        if (logger_)                                                                                            \
            logger_->write(finapp::logging::Level::Error,                                                       \
                           std::string("[gRPC ERROR] ") + __func__ + ": " + (e).what() + trace_);               \
        return grpc::Status{grpc::StatusCode::INTERNAL, (e).what()};                                            \
    } while (false)

// ===================================
// Static serialisation helpers
// ===================================

static void fillTimeSeries(finapp_rpc::TimeSeries* proto, const TimeSeriesView& view) {
    for (size_t i = 0; i < view.size(); ++i) {
        proto->add_timestamps_ms(view.timestamp(i));
        proto->add_values(view[i]);
    }
}

static void fillAnalysisStats(finapp_rpc::AnalysisStats* proto, const ::analysis::TimeSeriesAnalysis& stats) {
    proto->set_mean(stats.mean());
    proto->set_std_dev(stats.standardDeviation());
    proto->set_variance(stats.variance());
    proto->set_skewness(stats.skewness());
    proto->set_kurtosis(stats.kurtosis());
}

static void fillAssetAnalysis(finapp_rpc::AssetAnalysisResult* proto, finance::analysis::IAssetAnalysis& aa) {
    auto* assetId = proto->mutable_asset_id();
    assetId->set_ticker(aa.asset()->ticker());
    assetId->set_type(finapp_rpc::converters::toProto(aa.asset()->type()));

    fillTimeSeries(proto->mutable_price_series(), aa.session().sourceView());
    fillAnalysisStats(proto->mutable_price_stats(), aa.priceAnalysis());

    // "return" series — only available when the asset registered that transform (e.g. Equity).
    try {
        const auto& view = aa.session().derivedView("return");
        const auto& stats = aa.derivedAnalysis("return");

        auto* ns = proto->add_derived_series();
        ns->set_name("return");
        fillTimeSeries(ns->mutable_series(), view);

        auto* nstats = proto->add_derived_stats();
        nstats->set_name("return");
        fillAnalysisStats(nstats->mutable_stats(), stats);
    } catch (const std::exception&) {
        // Transform not registered — nothing was added to proto.
    }
}

static void fillPortfolioAnalysis(finapp_rpc::PortfolioAnalysisResult* proto, const std::string& portfolioId,
                                  finance::analysis::PortfolioAnalysis& pa) {
    proto->set_portfolio_id(portfolioId);
    proto->set_nav_mode(pa.navMode() == finance::analysis::NavMode::TargetWeighted ? finapp_rpc::TARGET_WEIGHTED
                                                                                   : finapp_rpc::QUANTITY_BASED);

    fillTimeSeries(proto->mutable_nav_series(), pa.navSeries());
    fillAnalysisStats(proto->mutable_nav_stats(), pa.navAnalysis());

    for (const auto& ticker : pa.tickers()) {
        proto->add_tickers(ticker);
        fillAssetAnalysis(proto->add_position_analyses(), *pa.assetAnalysis(ticker));
    }
}

// ===================================
// Constructor
// ===================================

PortfolioGrpcServiceImpl::PortfolioGrpcServiceImpl(
    std::shared_ptr<finapp::PortfolioService> portfolioService,
    std::shared_ptr<finapp::PortfolioAnalysisService> portfolioAnalysisService, finapp::logging::ILogger* logger)
    : portfolioService_{std::move(portfolioService)},
      portfolioAnalysisService_{std::move(portfolioAnalysisService)},
      logger_{finapp::logging::PrefixedLogger::wrap(logger, "PortfolioGrpcService")} {}

// ===================================
// Portfolio CRUD
// ===================================

grpc::Status PortfolioGrpcServiceImpl::ListPortfoliosSummary(grpc::ServerContext*,
                                                             const finapp_rpc::ListPortfoliosSummaryInput*,
                                                             finapp_rpc::ListPortfoliosSummaryOutput* reply) {
    try {
        for (const auto& id : portfolioService_->listPortfolioIds()) {
            auto meta = portfolioService_->loadMetadata(id);
            auto* ident = reply->add_listportfoliosidentification();
            ident->set_id(meta.id);
            ident->set_name(meta.name);
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::GetPortfoliosByIds(grpc::ServerContext*,
                                                          const finapp_rpc::GetPortfoliosByIdsInput* request,
                                                          finapp_rpc::GetPortfoliosByIdsOutput* reply) {
    try {
        const int64_t ts = request->timestampms();
        for (const auto& id : request->id()) {
            finance::Portfolio portfolio = portfolioService_->load(id);
            const auto overview = portfolioService_->computeOverviewAtTs(id, ts);
            *reply->add_listportfolios() =
                finapp_rpc::converters::toProto(portfolio, overview.totalValue, overview.weights);
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::CreatePortfolio(grpc::ServerContext*,
                                                       const finapp_rpc::CreatePortfolioInput* request,
                                                       finapp_rpc::CreatePortfolioOutput* reply) {
    try {
        const std::string id = std::to_string(request->timestampms()) + "_" + request->name();
        finance::Currency base = finapp_rpc::converters::fromProto(request->basecurrency());
        portfolioService_->createNew(id, request->name(), base);
        reply->set_id(id);
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
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
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

// ===================================
// NAV time series
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
        const auto overview = portfolioService_->computeOverviewAtTs(id, endMs);
        *reply->mutable_portfolio() = finapp_rpc::converters::toProto(portfolio, overview.totalValue, overview.weights);

        const TimeSeries vs = portfolioService_->valueSeries(id, startMs, endMs, deltaMs);
        auto* ts = reply->mutable_time_series();
        for (int64_t t : vs.getTimestamps()) ts->add_timestamps_ms(t);
        for (double v : vs.getValues()) ts->add_values(v);

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

// ===================================
// Stateless analysis
// ===================================

grpc::Status PortfolioGrpcServiceImpl::ComputePortfolioAnalysis(
    grpc::ServerContext*, const finapp_rpc::ComputePortfolioAnalysisInput* request,
    finapp_rpc::ComputePortfolioAnalysisOutput* reply) {
    try {
        const finance::Portfolio portfolio = portfolioService_->load(request->portfolio_id());
        auto pa = portfolioAnalysisService_->createPortfolioAnalysis(
            portfolio, request->start_ms(), request->end_ms(), request->frequency_ms());
        fillPortfolioAnalysis(reply->mutable_result(), portfolio.id(), *pa);
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

// ===================================
// Stateful analysis session
// ===================================

std::string PortfolioGrpcServiceImpl::makeHandle_(const char* prefix) {
    const auto nowUs =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    return std::string(prefix) + std::to_string(nowUs);
}

grpc::Status PortfolioGrpcServiceImpl::OpenPortfolioAnalysisSession(
    grpc::ServerContext*, const finapp_rpc::OpenPortfolioAnalysisSessionInput* request,
    finapp_rpc::OpenPortfolioAnalysisSessionOutput* reply) {
    try {
        const std::string portfolioId = request->portfolio_id();
        finance::Portfolio portfolio = portfolioService_->load(portfolioId);
        auto pa = portfolioAnalysisService_->createPortfolioAnalysis(
            portfolio, request->start_ms(), request->end_ms(), request->frequency_ms());

        if (pa->navMode() == finance::analysis::NavMode::QuantityBased) {
            auto nav = portfolioService_->valueSeries(
                portfolioId, request->start_ms(), request->end_ms(), request->frequency_ms());
            pa->setNavTimeSeries(std::make_shared<TimeSeries>(std::move(nav)));
        }

        const std::string handle = makeHandle_("pa_");
        auto [it, inserted] =
            sessions_.insert_or_assign(handle, SessionEntry{portfolioId, std::move(portfolio), std::move(pa)});

        reply->mutable_handle()->set_id(handle);
        fillPortfolioAnalysis(reply->mutable_result(), portfolioId, *it->second.analysis);
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::UpdatePortfolioAnalysisSessionRange(
    grpc::ServerContext*, const finapp_rpc::UpdateSessionRangeInput* request,
    finapp_rpc::UpdatePortfolioAnalysisSessionOutput* reply) {
    try {
        const std::string handle = request->handle().id();
        auto it = sessions_.find(handle);
        if (it == sessions_.end())
            return grpc::Status{grpc::StatusCode::NOT_FOUND, "Unknown session handle: " + handle};

        auto& entry = it->second;
        entry.analysis->setRange(request->start_ms(), request->end_ms());
        fillPortfolioAnalysis(reply->mutable_result(), entry.portfolioId, *entry.analysis);
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::UpdatePortfolioAnalysisSessionTimestamp(
    grpc::ServerContext*, const finapp_rpc::UpdatePortfolioSessionTimestampInput* request,
    finapp_rpc::UpdatePortfolioSessionTimestampOutput* reply) {
    try {
        const std::string handle = request->handle().id();
        auto it = sessions_.find(handle);
        if (it == sessions_.end())
            return grpc::Status{grpc::StatusCode::NOT_FOUND, "Unknown session handle: " + handle};

        const auto& entry = it->second;
        const auto overview = portfolioService_->computeOverviewAtTs(entry.portfolioId, request->timestamp_ms());
        *reply->mutable_snapshot() =
            finapp_rpc::converters::toProto(entry.portfolio, overview.totalValue, overview.weights);
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::ClosePortfolioAnalysisSession(grpc::ServerContext*,
                                                                     const finapp_rpc::CloseSessionInput* request,
                                                                     finapp_rpc::CloseSessionOutput*) {
    try {
        const std::string handle = request->handle().id();
        if (sessions_.erase(handle) == 0)
            return grpc::Status{grpc::StatusCode::NOT_FOUND, "Unknown session handle: " + handle};
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

// ===================================
// Asset session extraction
// ===================================

grpc::Status PortfolioGrpcServiceImpl::ExtractAssetAnalysisFromPortfolioSession(
    grpc::ServerContext*, const finapp_rpc::ExtractAssetFromPortfolioSessionInput* request,
    finapp_rpc::ExtractAssetFromPortfolioSessionOutput* reply) {
    try {
        const std::string portfolioHandle = request->portfolio_handle().id();
        auto it = sessions_.find(portfolioHandle);
        if (it == sessions_.end())
            return grpc::Status{grpc::StatusCode::NOT_FOUND, "Unknown portfolio session: " + portfolioHandle};

        auto assetAnalysis = it->second.analysis->assetAnalysis(request->ticker());

        const std::string assetHandle = makeHandle_("aa_");
        assetSessions_[assetHandle] = AssetSessionEntry{assetAnalysis};

        reply->mutable_asset_handle()->set_id(assetHandle);
        fillAssetAnalysis(reply->mutable_result(), *assetAnalysis);
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::CloseAssetSession(grpc::ServerContext*,
                                                         const finapp_rpc::CloseSessionInput* request,
                                                         finapp_rpc::CloseSessionOutput*) {
    try {
        const std::string handle = request->handle().id();
        if (assetSessions_.erase(handle) == 0)
            return grpc::Status{grpc::StatusCode::NOT_FOUND, "Unknown asset session: " + handle};
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
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
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::RequestAddTransaction(grpc::ServerContext*,
                                                             const finapp_rpc::RequestAddTransactionInput* request,
                                                             finapp_rpc::RequestAddTransactionOutput* reply) {
    try {
        finance::Transaction transaction = finapp_rpc::converters::fromProto(request->transaction());
        const std::string transactionId = portfolioService_->addTransaction(request->portfolioid(), transaction);
        reply->set_transactionid(transactionId);
        return grpc::Status::OK;
    } catch (std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::RequestAddTransactionByCsv(
    grpc::ServerContext*, const finapp_rpc::RequestAddTransactionByCsvInput* request,
    finapp_rpc::RequestAddTransactionOutput*) {
    try {
        auto meta = portfolioService_->loadMetadata(request->portfolioid());
        finapp::YahooFinanceImporter::Config config{meta.baseCurrency, nullptr};
        auto transactions = finapp::YahooFinanceImporter::parseFromString(request->csvdata(), config);
        if (transactions.empty())
            return grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "No valid transactions found in CSV data"};
        portfolioService_->importTransactions(request->portfolioid(), std::move(transactions));
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}

grpc::Status PortfolioGrpcServiceImpl::DeleteTransaction(grpc::ServerContext*,
                                                         const finapp_rpc::DeleteTransactionInput* request,
                                                         finapp_rpc::DeleteTransactionOutput* reply) {
    try {
        portfolioService_->deleteTransaction(request->portfolioid(), request->transactionid());
        reply->set_transactionid(request->transactionid());
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        GRPC_LOG_AND_RETURN_INTERNAL(e);
    }
}
