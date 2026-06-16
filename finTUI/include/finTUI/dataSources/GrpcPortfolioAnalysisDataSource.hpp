// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>

#include "finTUI/dataSources/IPortfolioAnalysisDataSource.hpp"
#include "portfolio.grpc.pb.h"

namespace finui {

class GrpcPortfolioAnalysisDataSource : public IPortfolioAnalysisDataSource {
 public:
    explicit GrpcPortfolioAnalysisDataSource(std::shared_ptr<grpc::Channel> channel);

    PortfolioAnalysisData computeAnalysis(const std::string& portfolioId, int64_t startMs, int64_t endMs,
                                          int64_t frequencyMs) override;

 private:
    std::unique_ptr<finapp_rpc::PortfolioService::Stub> stub_;
};

}  // namespace finui
