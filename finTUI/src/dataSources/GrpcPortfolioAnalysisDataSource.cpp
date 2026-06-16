// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/dataSources/GrpcPortfolioAnalysisDataSource.hpp"

#include <grpcpp/grpcpp.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "portfolio.pb.h"

namespace finui {

GrpcPortfolioAnalysisDataSource::GrpcPortfolioAnalysisDataSource(std::shared_ptr<grpc::Channel> channel)
    : stub_(finapp_rpc::PortfolioService::NewStub(std::move(channel))) {}

static TimeSeriesData protoToTimeSeries(const finapp_rpc::TimeSeries& ts) {
    TimeSeriesData out;
    out.timestamps.reserve(ts.timestamps_ms_size());
    out.values.reserve(ts.values_size());
    for (int64_t t : ts.timestamps_ms()) out.timestamps.push_back(t);
    for (double v : ts.values()) out.values.push_back(v);
    return out;
}

static AnalysisStatsData protoToStats(const finapp_rpc::AnalysisStats& s) {
    return {s.mean(), s.std_dev(), s.variance(), s.skewness(), s.kurtosis()};
}

PortfolioAnalysisData GrpcPortfolioAnalysisDataSource::computeAnalysis(const std::string& portfolioId,
                                                                        int64_t startMs, int64_t endMs,
                                                                        int64_t frequencyMs) {
    finapp_rpc::ComputePortfolioAnalysisInput req;
    req.set_portfolio_id(portfolioId);
    req.set_start_ms(startMs);
    req.set_end_ms(endMs);
    req.set_frequency_ms(frequencyMs);

    finapp_rpc::ComputePortfolioAnalysisOutput reply;
    grpc::ClientContext ctx;
    auto status = stub_->ComputePortfolioAnalysis(&ctx, req, &reply);
    if (!status.ok()) throw std::runtime_error("ComputePortfolioAnalysis: " + status.error_message());

    const auto& r = reply.result();
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

    for (const auto& row : r.price_correlation()) {
        std::vector<double> rowVec;
        for (double v : row.values()) rowVec.push_back(v);
        out.priceCorrelation.push_back(std::move(rowVec));
    }
    for (const auto& row : r.return_correlation()) {
        std::vector<double> rowVec;
        for (double v : row.values()) rowVec.push_back(v);
        out.returnCorrelation.push_back(std::move(rowVec));
    }
    for (const auto& row : r.covariance()) {
        std::vector<double> rowVec;
        for (double v : row.values()) rowVec.push_back(v);
        out.covariance.push_back(std::move(rowVec));
    }

    return out;
}

}  // namespace finui
