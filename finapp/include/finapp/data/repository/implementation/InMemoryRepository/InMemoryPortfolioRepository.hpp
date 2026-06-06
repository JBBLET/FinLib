// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/data/repository/interface/IPortfolioRepository.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finapp {

class InMemoryPortfolioRepository : public IPortfolioRepository {
 public:
    void saveSnapshot(const finance::PortfolioSnapshot& snapshot) override {
        auto& vec = snapshots_[snapshot.portfolioId];
        auto it = std::find_if(vec.begin(), vec.end(), [&](const finance::PortfolioSnapshot& s) {
            return s.timestampMs == snapshot.timestampMs;
        });
        if (it != vec.end()) {
            *it = snapshot;
        } else {
            vec.push_back(snapshot);
            std::sort(vec.begin(), vec.end(), [](const finance::PortfolioSnapshot& a, const finance::PortfolioSnapshot& b) {
                return a.timestampMs < b.timestampMs;
            });
        }
    }

    std::optional<finance::PortfolioSnapshot> loadLatestSnapshot(const std::string& portfolioId) const override {
        auto it = snapshots_.find(portfolioId);
        if (it == snapshots_.end() || it->second.empty()) return std::nullopt;
        return it->second.back();
    }

    std::vector<finance::PortfolioSnapshot> loadAllSnapshots(const std::string& portfolioId) const override {
        auto it = snapshots_.find(portfolioId);
        if (it == snapshots_.end()) return {};
        return it->second;
    }

    void replaceSnapshotsFrom(const std::string& portfolioId, int64_t fromTimestampMs,
                              const std::vector<finance::PortfolioSnapshot>& newSnapshots) override {
        auto& vec = snapshots_[portfolioId];
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [fromTimestampMs](const finance::PortfolioSnapshot& s) {
                                     return s.timestampMs >= fromTimestampMs;
                                 }),
                  vec.end());
        vec.insert(vec.end(), newSnapshots.begin(), newSnapshots.end());
        std::sort(vec.begin(), vec.end(), [](const finance::PortfolioSnapshot& a, const finance::PortfolioSnapshot& b) {
            return a.timestampMs < b.timestampMs;
        });
    }

    void appendTransactions(const std::string& portfolioId,
                            const std::vector<finance::Transaction>& transactions) override {
        auto& existing = transactions_[portfolioId];
        existing.insert(existing.end(), transactions.begin(), transactions.end());
    }

    std::vector<finance::Transaction> loadTransactions(const std::string& portfolioId,
                                                       int64_t afterTimestamps = 0) const override {
        auto it = transactions_.find(portfolioId);
        if (it == transactions_.end()) return {};
        std::vector<finance::Transaction> out;
        for (const auto& tx : it->second) {
            if (tx.timestampsMs >= afterTimestamps) out.push_back(tx);
        }
        return out;
    }

    std::vector<std::string> listPortfolioIds() const override {
        std::vector<std::string> out;
        for (const auto& [id, vec] : snapshots_) {
            if (!vec.empty()) out.push_back(id);
        }
        return out;
    }

    bool exists(const std::string& portfolioId) const override {
        auto it = snapshots_.find(portfolioId);
        if (it != snapshots_.end() && !it->second.empty()) return true;
        return transactions_.contains(portfolioId);
    }

    void deletePortfolio(const std::string& portfolioId) override {
        if (!exists(portfolioId)) {
            throw std::runtime_error("InMemoryPortfolioRepository: portfolio not found: " + portfolioId);
        }
        snapshots_.erase(portfolioId);
        transactions_.erase(portfolioId);
    }

    void deleteTransaction(const std::string& portfolioId, const std::string& transactionId) override {
        auto it = transactions_.find(portfolioId);
        if (it == transactions_.end()) {
            throw std::runtime_error("InMemoryPortfolioRepository: transaction not found: " + transactionId);
        }
        auto& txns = it->second;
        auto txIt = std::find_if(txns.begin(), txns.end(),
                                 [&](const finance::Transaction& t) { return t.id == transactionId; });
        if (txIt == txns.end()) {
            throw std::runtime_error("InMemoryPortfolioRepository: transaction not found: " + transactionId);
        }
        txns.erase(txIt);
    }

 private:
    std::unordered_map<std::string, std::vector<finance::PortfolioSnapshot>> snapshots_;
    std::unordered_map<std::string, std::vector<finance::Transaction>> transactions_;
};

}  // namespace finapp
