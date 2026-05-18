// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <grpcpp/grpcpp.h>

#include <memory>

#include "finui/IPortfolioDataSource.hpp"
#include "portfolio.grpc.pb.h"

namespace finui {

class GrpcPortfolioDataSource : public IPortfolioDataSource {
 public:
    explicit GrpcPortfolioDataSource(std::shared_ptr<grpc::Channel> channel);

    std::vector<std::string>    listPortfolioIds()                               override;
    PortfolioSummary            loadSummary(const std::string& portfolioId)      override;
    std::vector<TransactionRow> listTransactions(const std::string& portfolioId) override;

 private:
    std::unique_ptr<finapp_rpc::PortfolioService::Stub> stub_;
};

}  // namespace finui
