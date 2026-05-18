// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finui/GrpcPortfolioDataSource.hpp"

#include <chrono>
#include <stdexcept>

namespace finui {

GrpcPortfolioDataSource::GrpcPortfolioDataSource(std::shared_ptr<grpc::Channel> channel)
    : stub_(finapp_rpc::PortfolioService::NewStub(std::move(channel))) {}

std::vector<std::string> GrpcPortfolioDataSource::listPortfolioIds() {
    finapp_rpc::ListPortfoliosSummaryInput  req;
    finapp_rpc::ListPortfoliosSummaryOutput reply;
    grpc::ClientContext ctx;

    auto status = stub_->ListPortfoliosSummary(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("ListPortfoliosSummary: " + status.error_message());

    std::vector<std::string> ids;
    ids.reserve(reply.listportfoliosidentification_size());
    for (const auto& ident : reply.listportfoliosidentification()) ids.push_back(ident.id());
    return ids;
}

PortfolioSummary GrpcPortfolioDataSource::loadSummary(const std::string& portfolioId) {
    const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
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
    summary.id           = p.portfolioidentification().id();
    summary.name         = p.portfolioidentification().name();
    summary.baseCurrency = finapp_rpc::Currency_Name(p.basecurrency());
    summary.totalValue   = p.totalvalue();

    for (const auto& cash : p.cashbalances()) {
        summary.cashBalances.push_back({finapp_rpc::Currency_Name(cash.currency()), cash.amount()});
    }
    for (const auto& pos : p.positions()) {
        summary.positions.push_back({pos.ticker(), pos.quantity(), pos.value(), pos.weight()});
    }
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

}  // namespace finui
