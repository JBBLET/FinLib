// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/data/repository/interface/IAssetRepository.hpp"
#include "finapp/finance/asset/Cash.hpp"
#include "finapp/finance/asset/IAsset.hpp"
namespace finapp {

class CSVCashRepository : public IAssetRepository {
 public:
    explicit CSVCashRepository(std::filesystem::path directory);

    void save(const std::shared_ptr<const finance::IAsset>& asset) override;
    std::shared_ptr<const finance::IAsset> load(const std::string& ticker) const override;

    bool exists(const std::string& ticker) const override;
    std::vector<std::string> listTickers() const override;

    std::unordered_map<std::string, std::shared_ptr<const finance::IAsset>> loadAll(
        const std::vector<std::string>& tickers) const override;

 private:
    std::filesystem::path directory_;
    finance::AssetType assetType_ = finance::AssetType::Cash;
    std::filesystem::path csvPath_(const std::string& ticker) const;

    std::shared_ptr<finance::Cash> readCsv_(const std::string& ticker) const;
    void writeCsv_(const std::shared_ptr<const finance::Cash>& asset) const;

    static std::shared_ptr<finance::Cash> parseCsvFile_(const std::filesystem::path& path, const std::string& ticker);
};
}  // namespace finapp
