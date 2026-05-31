// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/dataSources/GrpcPortfolioDataSource.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "portfolio.pb.h"

namespace {

const std::unordered_map<std::string, finapp_rpc::Currency> rpcCurrencyFromStr = {
    {"USD", finapp_rpc::Currency::USD},
    {"EUR", finapp_rpc::Currency::EUR},
    {"JPY", finapp_rpc::Currency::JPY},
    {"KRW", finapp_rpc::Currency::KRW},
    {"CAD", finapp_rpc::Currency::CAD},
    {"GBP", finapp_rpc::Currency::GBP},
};

const std::unordered_map<std::string, finapp_rpc::TransactionType> rpcTransactionTypeFromStr = {
    {"BUY", finapp_rpc::TransactionType::BUY},
    {"SELL", finapp_rpc::TransactionType::SELL},
    {"DEPOSIT", finapp_rpc::TransactionType::DEPOSIT},
    {"WITHDRAWAL", finapp_rpc::TransactionType::WITHDRAWAL},
    {"DIVIDEND", finapp_rpc::TransactionType::DIVIDEND},
    {"SPLIT", finapp_rpc::TransactionType::SPLIT},
};

}  // namespace

namespace finui {

GrpcPortfolioDataSource::GrpcPortfolioDataSource(std::shared_ptr<grpc::Channel> channel)
    : stub_(finapp_rpc::PortfolioService::NewStub(std::move(channel))) {}

std::vector<PortfolioListEntry> GrpcPortfolioDataSource::listPortfolios() {
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

PortfolioSummary GrpcPortfolioDataSource::loadSummary(const std::string& portfolioId) {
    const int64_t nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    finapp_rpc::GetPortfoliosByIdsInput req;
    req.add_id(portfolioId);
    req.set_timestampms(nowMs);

    finapp_rpc::GetPortfoliosByIdsOutput reply;
    grpc::ClientContext ctx;

    auto status = stub_->GetPortfoliosByIds(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("GetPortfoliosByIds: " + status.error_message());
    if (reply.listportfolios().empty()) throw std::runtime_error("Portfolio not found: " + portfolioId);

    const auto& p = reply.listportfolios(0);

    PortfolioSummary summary;
    summary.id = p.portfolioidentification().id();
    summary.name = p.portfolioidentification().name();
    summary.baseCurrency = finapp_rpc::Currency_Name(p.basecurrency());
    summary.totalValue = p.totalvalue();

    for (const auto& cash : p.cashbalances())
        summary.cashBalances.push_back({finapp_rpc::Currency_Name(cash.currency()), cash.amount()});
    for (const auto& pos : p.positions())
        summary.positions.push_back({pos.ticker(), pos.quantity(), pos.value(), pos.weight()});
    return summary;
}

std::vector<TransactionRow> GrpcPortfolioDataSource::listTransactions(const std::string& portfolioId) {
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

std::string GrpcPortfolioDataSource::createPortfolio(const CreatePortfolioParams& p) {
    finapp_rpc::CreatePortfolioInput req;
    req.set_name(p.name);
    req.set_timestampms(p.timestampsMs);
    auto cit = rpcCurrencyFromStr.find(p.currency);
    if (cit != rpcCurrencyFromStr.end()) req.set_basecurrency(cit->second);

    finapp_rpc::CreatePortfolioOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->CreatePortfolio(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("CreatePortfolio: " + status.error_message());
    return reply.id();
}

void GrpcPortfolioDataSource::deletePortfolio(const std::string& portfolioId) {
    finapp_rpc::DeletePortfolioByIdInput req;
    req.set_id(portfolioId);
    finapp_rpc::DeletePortfolioByIdOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->DeletePortfolioById(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("DeletePortfolio: " + status.error_message());
}

std::string GrpcPortfolioDataSource::addTransaction(const std::string& portfolioId, const AddTransactionParams& p) {
    finapp_rpc::RequestAddTransactionInput req;
    req.set_portfolioid(portfolioId);
    auto* t = req.mutable_transaction();
    t->set_timestampsms(p.timestampMs);

    auto tit = rpcTransactionTypeFromStr.find(p.type);
    if (tit != rpcTransactionTypeFromStr.end()) t->set_type(tit->second);

    const bool isCash = (p.type == "DEPOSIT" || p.type == "WITHDRAWAL");
    t->set_assettype(isCash ? finapp_rpc::AssetType::CASH : finapp_rpc::AssetType::EQUITY);
    t->set_assetticker(isCash ? p.currency : p.ticker);

    t->set_quantity(p.quantity);
    t->set_priceperunit(p.pricePerUnit);
    t->set_fees(p.fees);

    auto cit = rpcCurrencyFromStr.find(p.currency);
    if (cit != rpcCurrencyFromStr.end()) t->set_settlementcurrency(cit->second);

    finapp_rpc::RequestAddTransactionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->RequestAddTransaction(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("RequestAddTransaction: " + status.error_message());
    return reply.transactionid();
}

void GrpcPortfolioDataSource::importCsv(const std::string& portfolioId, const std::string& csvData) {
    finapp_rpc::RequestAddTransactionByCsvInput req;
    req.set_portfolioid(portfolioId);
    req.set_csvdata(csvData);
    finapp_rpc::RequestAddTransactionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->RequestAddTransactionByCsv(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("ImportCsv: " + status.error_message());
}

void GrpcPortfolioDataSource::deleteTransaction(const std::string& portfolioId, const std::string& txnId) {
    finapp_rpc::DeleteTransactionInput req;
    req.set_portfolioid(portfolioId);
    req.set_transactionid(txnId);
    finapp_rpc::DeleteTransactionOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->DeleteTransaction(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("DeleteTransaction: " + status.error_message());
}

TimeSeriesData GrpcPortfolioDataSource::getTimeSeries(const std::string& portfolioId, int64_t startMs, int64_t endMs,
                                                      int64_t deltaMs) {
    finapp_rpc::GetPortfolioTimeSeriesByIdInput req;
    req.set_id(portfolioId);
    req.set_startms(startMs);
    req.set_endms(endMs);
    req.set_deltatms(deltaMs);

    finapp_rpc::GetPortfolioTimeSeriesByIdOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->GetPortfolioTimeSeriesById(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("GetTimeSeries: " + status.error_message());

    TimeSeriesData result;
    const auto& ts = reply.timeseries();
    for (int64_t t : ts.timestampsms()) result.timestamps.push_back(t);
    for (double v : ts.closevalues()) result.values.push_back(v);
    return result;
}

}  // namespace finui
