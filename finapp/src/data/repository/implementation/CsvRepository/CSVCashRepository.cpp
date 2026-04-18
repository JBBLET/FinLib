// "Copyright (c) 2026 JBBLET All Rights Reserved."

#include "finapp/data/repository/implementation/CsvRepository/CSVCashRepository.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/finance/asset/Cash.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"

namespace finapp {

using namespace finance;

CSVCashRepository::CSVCashRepository(std::filesystem::path directory) : directory_(std::move(directory)) {}

std::filesystem::path CSVCashRepository::csvPath_(const std::string& ticker) const {
    return directory_ / (assetTypeToString(assetType_)) / (ticker + ".csv");
}

std::shared_ptr<Cash> CSVCashRepository::readCsv_(const std::string& ticker) const {
    auto path = csvPath_(ticker);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("CSV file not found:" + path.string());
    }
    return parseCsvFile_(path, ticker);
}

void CSVCashRepository::writeCsv_(const std::shared_ptr<const Cash>& asset) const {
    auto path = csvPath_(asset->ticker());
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file for writing: " + path.string());
    }
    file << "ticker,denomination\n";
    file << asset->ticker() + "," + toString(asset->denomination()) + "\n";
}

std::shared_ptr<Cash> CSVCashRepository::parseCsvFile_(const std::filesystem::path& path,
                                                       const std::string& ticker) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + path.string());
    }
    std::string tickerString, denomString;
    std::string line;
    std::getline(file, line);  // skip header
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        if (std::getline(iss, tickerString, ',') && std::getline(iss, denomString)) {
            Currency denomination = currencyFromString(denomString);
            return std::make_shared<Cash>(Cash(denomination));
        }
    }
    throw std::runtime_error("No data found in CSV file: " + path.string());
}

void CSVCashRepository::save(const std::shared_ptr<const IAsset>& asset) {
    auto cash = std::dynamic_pointer_cast<const Cash>(asset);
    if (!cash) {
        throw std::runtime_error("CSVCashRepository::save called with non-Cash asset");
    }
    writeCsv_(cash);
}

std::shared_ptr<const IAsset> CSVCashRepository::load(const std::string& ticker) const {
    return readCsv_(ticker);
}

bool CSVCashRepository::exists(const std::string& ticker) const { return std::filesystem::exists(csvPath_(ticker)); }

std::vector<std::string> CSVCashRepository::listTickers() const {
    std::filesystem::path path = directory_ / assetTypeToString(assetType_);
    std::vector<std::string> output;
    output.reserve(100);
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().extension() == ".csv") {
            output.push_back(entry.path().stem());
        }
    }
    return output;
}

std::unordered_map<std::string, std::shared_ptr<const IAsset>> CSVCashRepository::loadAll(
    const std::vector<std::string>& tickers) const {
    std::unordered_map<std::string, std::shared_ptr<const IAsset>> output;
    output.reserve(tickers.size());
    for (const auto& ticker : tickers) {
        output[ticker] = load(ticker);
    }
    return output;
}
}  // namespace finapp
