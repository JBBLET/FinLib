// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/dataSources/GrpcPortfolioSessionDataSource.hpp"

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "portfolio.pb.h"

namespace finui {

namespace {

static constexpr int64_t kDayMs = 86'400'000LL;
static constexpr int64_t kFiveYearsMs = 5LL * 365 * kDayMs;

const std::unordered_map<std::string, finapp_rpc::Currency> kCurrencyFromStr = {
    {"USD", finapp_rpc::Currency::USD},
    {"EUR", finapp_rpc::Currency::EUR},
    {"JPY", finapp_rpc::Currency::JPY},
    {"KRW", finapp_rpc::Currency::KRW},
    {"CAD", finapp_rpc::Currency::CAD},
    {"GBP", finapp_rpc::Currency::GBP},
};

const std::unordered_map<std::string, finapp_rpc::TransactionType> kTxnTypeFromStr = {
    {"BUY", finapp_rpc::TransactionType::BUY},
    {"SELL", finapp_rpc::TransactionType::SELL},
    {"DEPOSIT", finapp_rpc::TransactionType::DEPOSIT},
    {"WITHDRAWAL", finapp_rpc::TransactionType::WITHDRAWAL},
    {"DIVIDEND", finapp_rpc::TransactionType::DIVIDEND},
    {"SPLIT", finapp_rpc::TransactionType::SPLIT},
};

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

TimeSeriesData protoToTimeSeries(const finapp_rpc::TimeSeries& ts) {
    TimeSeriesData out;
    out.timestamps.reserve(ts.timestamps_ms_size());
    out.values.reserve(ts.values_size());
    for (int64_t t : ts.timestamps_ms()) out.timestamps.push_back(t);
    for (double v : ts.values()) out.values.push_back(v);
    return out;
}

AnalysisStatsData protoToStats(const finapp_rpc::AnalysisStats& s) {
    return {s.mean(), s.std_dev(), s.variance(), s.skewness(), s.kurtosis()};
}

PortfolioSummary protoToSummary(const finapp_rpc::Portfolio& p) {
    PortfolioSummary out;
    out.id = p.portfolioidentification().id();
    out.name = p.portfolioidentification().name();
    out.baseCurrency = finapp_rpc::Currency_Name(p.basecurrency());
    out.totalValue = p.totalvalue();
    for (const auto& cb : p.cashbalances())
        out.cashBalances.push_back({finapp_rpc::Currency_Name(cb.currency()), cb.amount()});
    for (const auto& pos : p.positions())
        out.positions.push_back({pos.ticker(), pos.quantity(), pos.value(), pos.weight()});
    return out;
}

}  // namespace

GrpcPortfolioSessionDataSource::GrpcPortfolioSessionDataSource(std::shared_ptr<grpc::Channel> channel)
    : stub_(finapp_rpc::PortfolioService::NewStub(std::move(channel))) {}

GrpcPortfolioSessionDataSource::~GrpcPortfolioSessionDataSource() { closeSession_(); }

// ---------------------------------------------------------------------------
// Session management
// ---------------------------------------------------------------------------

void GrpcPortfolioSessionDataSource::ensureSession_(const std::string& portfolioId) {
    if (sessionPortfolioId_ == portfolioId && !sessionHandle_.empty()) return;
    closeSession_();

    const int64_t now = nowMs();
    finapp_rpc::OpenPortfolioAnalysisSessionInput req;
    req.set_portfolio_id(portfolioId);
    req.set_start_ms(now - kFiveYearsMs);
    req.set_end_ms(now);
    req.set_frequency_ms(kDayMs);

    finapp_rpc::OpenPortfolioAnalysisSessionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->OpenPortfolioAnalysisSession(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("OpenPortfolioAnalysisSession: " + status.error_message());

    sessionHandle_ = reply.handle().id();
    sessionPortfolioId_ = portfolioId;
}

void GrpcPortfolioSessionDataSource::closeSession_() {
    if (sessionHandle_.empty()) return;
    finapp_rpc::CloseSessionInput req;
    req.mutable_handle()->set_id(sessionHandle_);
    finapp_rpc::CloseSessionOutput reply;
    grpc::ClientContext ctx;
    stub_->ClosePortfolioAnalysisSession(&ctx, req, &reply);
    sessionHandle_.clear();
    sessionPortfolioId_.clear();
}

// ---------------------------------------------------------------------------
// Analysis result extraction (shared between getTimeSeries and computeAnalysis)
// ---------------------------------------------------------------------------

PortfolioAnalysisData GrpcPortfolioSessionDataSource::extractAnalysis_(const finapp_rpc::PortfolioAnalysisResult& r) {
    PortfolioAnalysisData out;
    out.portfolioId = r.portfolio_id();
    out.navSeries = protoToTimeSeries(r.nav_series());
    out.navStats = protoToStats(r.nav_stats());

    for (const auto& pos : r.position_analyses()) {
        AssetAnalysisData asset;
        asset.ticker = pos.asset_id().ticker();
        asset.priceSeries = protoToTimeSeries(pos.price_series());
        asset.priceStats = protoToStats(pos.price_stats());
        for (const auto& ns : pos.derived_series()) {
            if (ns.name() == "return") asset.returnSeries = protoToTimeSeries(ns.series());
        }
        for (const auto& ns : pos.derived_stats()) {
            if (ns.name() == "return") asset.returnStats = protoToStats(ns.stats());
        }
        out.positions.push_back(std::move(asset));
    }

    return out;
}

// ---------------------------------------------------------------------------
// IPortfolioDataSource — read
// ---------------------------------------------------------------------------

std::vector<PortfolioListEntry> GrpcPortfolioSessionDataSource::listPortfolios() {
    finapp_rpc::ListPortfoliosSummaryInput req;
    finapp_rpc::ListPortfoliosSummaryOutput reply;
    grpc::ClientContext ctx;

    auto status = stub_->ListPortfoliosSummary(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("ListPortfoliosSummary: " + status.error_message());

    std::vector<PortfolioListEntry> entries;
    entries.reserve(reply.listportfoliosidentification_size());
    for (const auto& ident : reply.listportfoliosidentification()) entries.push_back({ident.id(), ident.name()});
    return entries;
}

PortfolioSummary GrpcPortfolioSessionDataSource::loadSummary(const std::string& portfolioId) {
    ensureSession_(portfolioId);

    finapp_rpc::UpdatePortfolioSessionTimestampInput req;
    req.mutable_handle()->set_id(sessionHandle_);
    req.set_timestamp_ms(nowMs());

    finapp_rpc::UpdatePortfolioSessionTimestampOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->UpdatePortfolioAnalysisSessionTimestamp(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("UpdatePortfolioAnalysisSessionTimestamp: " + status.error_message());

    return protoToSummary(reply.snapshot());
}

std::vector<TransactionRow> GrpcPortfolioSessionDataSource::listTransactions(const std::string& portfolioId) {
    finapp_rpc::ListPortfolioTransactionsByPortfolioIdInput req;
    req.set_portfolioid(portfolioId);

    finapp_rpc::ListPortfolioTransactionsByPortfolioIdOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->ListPortfolioTransactionsByPortfolioId(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("ListTransactions: " + status.error_message());

    std::vector<TransactionRow> rows;
    rows.reserve(reply.transactionlist_size());
    for (const auto& t : reply.transactionlist()) {
        rows.push_back({
            t.id(),
            t.timestampsms(),
            finapp_rpc::TransactionType_Name(t.type()),
            t.assetticker(),
            t.quantity(),
            t.priceperunit(),
            t.fees(),
            finapp_rpc::Currency_Name(t.settlementcurrency()),
        });
    }
    return rows;
}

TimeSeriesData GrpcPortfolioSessionDataSource::getTimeSeries(const std::string& portfolioId, int64_t startMs,
                                                             int64_t endMs, int64_t /*deltaMs*/) {
    ensureSession_(portfolioId);

    finapp_rpc::UpdateSessionRangeInput req;
    req.mutable_handle()->set_id(sessionHandle_);
    req.set_start_ms(startMs);
    req.set_end_ms(endMs);

    finapp_rpc::UpdatePortfolioAnalysisSessionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->UpdatePortfolioAnalysisSessionRange(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("UpdatePortfolioAnalysisSessionRange: " + status.error_message());

    return protoToTimeSeries(reply.result().nav_series());
}

// ---------------------------------------------------------------------------
// IPortfolioDataSource — write (invalidate session)
// ---------------------------------------------------------------------------

std::string GrpcPortfolioSessionDataSource::createPortfolio(const CreatePortfolioParams& p) {
    finapp_rpc::CreatePortfolioInput req;
    req.set_name(p.name);
    req.set_timestampms(p.timestampsMs);
    if (auto it = kCurrencyFromStr.find(p.currency); it != kCurrencyFromStr.end()) req.set_basecurrency(it->second);

    finapp_rpc::CreatePortfolioOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->CreatePortfolio(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("CreatePortfolio: " + status.error_message());
    return reply.id();
}

void GrpcPortfolioSessionDataSource::deletePortfolio(const std::string& portfolioId) {
    if (sessionPortfolioId_ == portfolioId) closeSession_();

    finapp_rpc::DeletePortfolioByIdInput req;
    req.set_id(portfolioId);
    finapp_rpc::DeletePortfolioByIdOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->DeletePortfolioById(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("DeletePortfolio: " + status.error_message());
}

std::string GrpcPortfolioSessionDataSource::addTransaction(const std::string& portfolioId,
                                                           const AddTransactionParams& p) {
    if (sessionPortfolioId_ == portfolioId) closeSession_();

    finapp_rpc::RequestAddTransactionInput req;
    req.set_portfolioid(portfolioId);
    auto* t = req.mutable_transaction();
    t->set_timestampsms(p.timestampMs);

    if (auto it = kTxnTypeFromStr.find(p.type); it != kTxnTypeFromStr.end()) t->set_type(it->second);

    const bool isCash = (p.type == "DEPOSIT" || p.type == "WITHDRAWAL");
    t->set_assettype(isCash ? finapp_rpc::AssetType::CASH : finapp_rpc::AssetType::EQUITY);
    t->set_assetticker(isCash ? p.currency : p.ticker);
    t->set_quantity(p.quantity);
    t->set_priceperunit(p.pricePerUnit);
    t->set_fees(p.fees);

    if (auto it = kCurrencyFromStr.find(p.currency); it != kCurrencyFromStr.end())
        t->set_settlementcurrency(it->second);

    finapp_rpc::RequestAddTransactionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->RequestAddTransaction(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("RequestAddTransaction: " + status.error_message());
    return reply.transactionid();
}

void GrpcPortfolioSessionDataSource::deleteTransaction(const std::string& portfolioId, const std::string& txnId) {
    if (sessionPortfolioId_ == portfolioId) closeSession_();

    finapp_rpc::DeleteTransactionInput req;
    req.set_portfolioid(portfolioId);
    req.set_transactionid(txnId);
    finapp_rpc::DeleteTransactionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->DeleteTransaction(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("DeleteTransaction: " + status.error_message());
}

void GrpcPortfolioSessionDataSource::importCsv(const std::string& portfolioId, const std::string& csvData) {
    if (sessionPortfolioId_ == portfolioId) closeSession_();

    finapp_rpc::RequestAddTransactionByCsvInput req;
    req.set_portfolioid(portfolioId);
    req.set_csvdata(csvData);
    finapp_rpc::RequestAddTransactionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->RequestAddTransactionByCsv(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("ImportCsv: " + status.error_message());
}

// ---------------------------------------------------------------------------
// IPortfolioAnalysisDataSource
// ---------------------------------------------------------------------------

PortfolioAnalysisData GrpcPortfolioSessionDataSource::computeAnalysis(const std::string& portfolioId, int64_t startMs,
                                                                      int64_t endMs, int64_t /*frequencyMs*/) {
    ensureSession_(portfolioId);

    finapp_rpc::UpdateSessionRangeInput req;
    req.mutable_handle()->set_id(sessionHandle_);
    req.set_start_ms(startMs);
    req.set_end_ms(endMs);

    finapp_rpc::UpdatePortfolioAnalysisSessionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->UpdatePortfolioAnalysisSessionRange(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("UpdatePortfolioAnalysisSessionRange: " + status.error_message());

    return extractAnalysis_(reply.result());
}

}  // namespace finui
