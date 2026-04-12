// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "finapp/data/repository/interface/IFXRepository.hpp"
#include "finapp/finance/common/Currency.hpp"
namespace finance {

class CSVFXRepository : public IFXRepository {
 public:
    explicit CSVFXRepository(std::filesystem::path directory);

    FXInfos load(const Currency& baseCurrency, const Currency& quoteCurrency) const override;
    void save(const FXInfos& fxInfos) override;
    bool exists(const Currency& baseCurrency, const Currency& quoteCurrency) const override;

 private:
    std::filesystem::path directory_;
    std::filesystem::path csvPath_() const;

    std::vector<FXInfos> readAll_() const;
    void writeAll_(const std::vector<FXInfos>& entries) const;
};
}  // namespace finance
