#pragma once

#include <string>
#include <unordered_map>

#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "portfolio.pb.h"

namespace finapp_rpc::converters {

// ---- Enum helpers ----
// Safe because proto enum ordinals are defined to match the C++ enum ordinals.

inline finapp_rpc::Currency toProto(finance::Currency c) {
    return static_cast<finapp_rpc::Currency>(static_cast<int>(c));
}
inline finance::Currency fromProto(finapp_rpc::Currency c) {
    return static_cast<finance::Currency>(static_cast<int>(c));
}

inline finapp_rpc::TransactionType toProto(finance::TransactionType t) {
    return static_cast<finapp_rpc::TransactionType>(static_cast<int>(t));
}
inline finance::TransactionType fromProto(finapp_rpc::TransactionType t) {
    return static_cast<finance::TransactionType>(static_cast<int>(t));
}

inline finapp_rpc::AssetType toProto(finance::AssetType a) {
    return static_cast<finapp_rpc::AssetType>(static_cast<int>(a));
}
inline finance::AssetType fromProto(finapp_rpc::AssetType a) {
    return static_cast<finance::AssetType>(static_cast<int>(a));
}

// ---- Transaction ----

inline finapp_rpc::Transaction toProto(const finance::Transaction& t) {
    finapp_rpc::Transaction proto;
    proto.set_id(t.id);
    proto.set_timestampsms(t.timestampsMs);
    proto.set_type(toProto(t.type));
    proto.set_assettype(toProto(t.assetType));
    proto.set_assetticker(t.assetTicker);
    proto.set_quantity(t.quantity);
    proto.set_priceperunit(t.pricePerUnit);
    proto.set_fees(t.fees);
    proto.set_settlementcurrency(toProto(t.settlementCurrency));
    return proto;
}

inline finance::Transaction fromProto(const finapp_rpc::Transaction& proto) {
    return finance::Transaction{
        .id                 = proto.id(),
        .timestampsMs       = proto.timestampsms(),
        .type               = fromProto(proto.type()),
        .assetType          = fromProto(proto.assettype()),
        .assetTicker        = proto.assetticker(),
        .quantity           = proto.quantity(),
        .pricePerUnit       = proto.priceperunit(),
        .fees               = proto.fees(),
        .settlementCurrency = fromProto(proto.settlementcurrency()),
    };
}

// ---- Portfolio ----
// totalValue and weights must be pre-computed by the gRPC service impl via
// PortfolioService::totalValue() and PortfolioService::weights() before calling this.
// weights() keys equity positions by ticker and cash by "CASH:<currency>".

inline finapp_rpc::Portfolio toProto(const finance::Portfolio& portfolio, double totalValue,
                                     const std::unordered_map<std::string, double>& weights) {
    finapp_rpc::Portfolio proto;
    proto.mutable_portfolioidentification()->set_id(portfolio.id());
    proto.mutable_portfolioidentification()->set_name(portfolio.name());
    proto.set_basecurrency(toProto(portfolio.baseCurrency()));
    proto.set_totalvalue(totalValue);

    for (const auto& [currency, amount] : portfolio.cashBalances()) {
        auto* cb = proto.add_cashbalances();
        cb->set_currency(toProto(currency));
        cb->set_amount(amount);
    }

    for (const auto& pos : portfolio.positions()) {
        auto* p = proto.add_positions();
        p->set_ticker(pos.assetId.ticker);
        p->set_quantity(pos.quantity);
        if (auto it = weights.find(pos.assetId.ticker); it != weights.end()) {
            p->set_weight(it->second);
            p->set_value(it->second * totalValue);
        }
    }

    return proto;
}

}  // namespace finapp_rpc::converters
