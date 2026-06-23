// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/analysisService/AssetsAnalysis/EquityAnalysisService.hpp"

#include <memory>
#include <utility>

#include "finapp/common/Exception.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finapp/finance/analysis/EquityAnalysis.hpp"
#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {

EquityAnalysisService::EquityAnalysisService(std::shared_ptr<AssetService> assetService,
                                             finapp::logging::ILogger* logger)
    : assetService_{std::move(assetService)},
      logger_{finapp::logging::PrefixedLogger::wrap(logger, "EquityAnalysisService")} {}

std::shared_ptr<finance::analysis::IAssetAnalysis> EquityAnalysisService::createAnalysisFromSession(
    std::shared_ptr<const finance::IAsset> asset, std::shared_ptr<::analysis::TimeSeriesSession> session) {
    if (auto equityPtr = std::dynamic_pointer_cast<const finance::Equity>(asset)) {
        if (logger_)
            logger_->write(finapp::logging::Level::Debug, "createAnalysisFromSession '" + asset->ticker() + "'");
        return std::make_shared<finance::analysis::EquityAnalysis>(
            finance::analysis::EquityAnalysis(equityPtr, session, assetService_));
    } else {
        throw finapp::Exception("IAsset to Equity Cast Error!");
    }
}
}  // namespace finapp
