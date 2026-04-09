// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/data/repository/interface/IAssetRepository.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
namespace finance {

class CSVEquityRepository : public IAssetRepository {
 public:
    explicit CSVEquityRepository(std::filesystem::path directory);

    void save(const std::shared_ptr<const IAsset>& asset) override;
    std::shared_ptr<const IAsset> load(const std::string& ticker) const override;

    bool exists(const std::string& ticker) const override;
    std::vector<std::string> listTickers() const override;

    std::unordered_map<std::string, std::shared_ptr<const IAsset>> loadAll(
        const std::vector<std::string>& tickers) const override;

 private:
    std::filesystem::path directory_;
    AssetType assetType_ = AssetType::Equity;
    std::filesystem::path csvPath_(const std::string& ticker) const;
    std::filesystem::path attributePath_(const std::string& ticker) const;

    std::shared_ptr<Equity> readCsv_(const std::string& ticker) const;
    std::unordered_map<std::string, std::string> readAttribute_(const std::string& ticker) const;
    void writeCsv_(const std::shared_ptr<const Equity>& asset) const;
    void writeAttributes_(const std::shared_ptr<const Equity>& asset) const;

    static std::shared_ptr<Equity> parseCsvFile_(const std::filesystem::path& path, const std::string& ticker);
    static std::unordered_map<std::string, std::string> parseAttributeFile_(const std::filesystem::path& path);
};
}  // namespace finance
