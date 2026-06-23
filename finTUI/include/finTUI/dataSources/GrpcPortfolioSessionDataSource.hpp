// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <grpcpp/grpcpp.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "finTUI/dataSources/IPortfolioAnalysisDataSource.hpp"
#include "finTUI/dataSources/IPortfolioDataSource.hpp"
#include "portfolio.grpc.pb.h"

namespace finui {

// Unified data source that implements both IPortfolioDataSource and
// IPortfolioAnalysisDataSource via a single stateful server-side session.
//
// A session is opened lazily on the first loadSummary()/getTimeSeries()/
// computeAnalysis() call for a portfolio. All subsequent calls on the same
// portfolio reuse it — range updates are cheap (no re-fetch). Write operations
// (add/delete transaction, create/delete portfolio) invalidate the session so
// the next read re-opens with fresh data.
class GrpcPortfolioSessionDataSource : public IPortfolioDataSource, public IPortfolioAnalysisDataSource {
 public:
    explicit GrpcPortfolioSessionDataSource(std::shared_ptr<grpc::Channel> channel);
    ~GrpcPortfolioSessionDataSource() override;

    // --- IPortfolioDataSource ---
    std::vector<PortfolioListEntry> listPortfolios() override;
    PortfolioSummary loadSummary(const std::string& portfolioId) override;
    std::vector<TransactionRow> listTransactions(const std::string& portfolioId) override;
    std::string createPortfolio(const CreatePortfolioParams& p) override;
    void deletePortfolio(const std::string& portfolioId) override;
    std::string addTransaction(const std::string& portfolioId, const AddTransactionParams& p) override;
    void deleteTransaction(const std::string& portfolioId, const std::string& txnId) override;
    TimeSeriesData getTimeSeries(const std::string& portfolioId, int64_t startMs, int64_t endMs,
                                 int64_t deltaMs) override;
    void importCsv(const std::string& portfolioId, const std::string& csvData) override;
    void onPortfolioDeselected() override;

    // --- IPortfolioAnalysisDataSource ---
    PortfolioAnalysisData computeAnalysis(const std::string& portfolioId, int64_t startMs, int64_t endMs,
                                          int64_t frequencyMs) override;

 private:
    std::unique_ptr<finapp_rpc::PortfolioService::Stub> stub_;

    std::string sessionHandle_;
    std::string sessionPortfolioId_;

    // Opens a session for portfolioId if not already open for that portfolio.
    // Closes any existing session first.
    void ensureSession_(const std::string& portfolioId);
    void closeSession_();

    PortfolioAnalysisData extractAnalysis_(const finapp_rpc::PortfolioAnalysisResult& r);
};

}  // namespace finui
