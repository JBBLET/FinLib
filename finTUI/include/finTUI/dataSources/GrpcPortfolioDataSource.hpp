// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>
#include <vector>

#include "finTUI/dataSources/IPortfolioDataSource.hpp"
#include "portfolio.grpc.pb.h"

namespace finui {

class GrpcPortfolioDataSource : public IPortfolioDataSource {
 public:
    explicit GrpcPortfolioDataSource(std::shared_ptr<grpc::Channel> channel);

    std::vector<PortfolioListEntry> listPortfolios() override;
    PortfolioSummary loadSummary(const std::string& portfolioId) override;
    std::vector<TransactionRow> listTransactions(const std::string& portfolioId) override;
    std::string createPortfolio(const CreatePortfolioParams& p) override;
    void deletePortfolio(const std::string& portfolioId) override;
    std::string addTransaction(const std::string& portfolioId, const AddTransactionParams& p) override;
    void deleteTransaction(const std::string& portfolioId, const std::string& txnId) override;
    TimeSeriesData getTimeSeries(const std::string& portfolioId, int64_t startMs, int64_t endMs,
                                 int64_t deltaMs) override;

 private:
    std::unique_ptr<finapp_rpc::PortfolioService::Stub> stub_;
};

}  // namespace finui
