// "Copyright (c) 2026 JBBLET All Rights Reserved."

#include "finapp/data/repository/implementation/CsvRepository/CSVEquityRepository.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"

namespace finance {

CSVEquityRepository::CSVEquityRepository(std::filesystem::path directory) : directory_(std::move(directory)) {}

std::filesystem::path CSVEquityRepository::csvPath_(const std::string& ticker) const {
    return directory_ / (assetTypeToString(assetType_)) / (ticker + ".csv");
}

std::filesystem::path CSVEquityRepository::attributePath_(const std::string& ticker) const {
    return directory_ / (assetTypeToString(assetType_)) / (ticker + ".attr");
}

std::shared_ptr<Equity> CSVEquityRepository::readCsv_(const std::string& ticker) const {
    auto path = csvPath_(ticker);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("CSV file not found:" + path.string());
    }
    return parseCsvFile_(path, ticker);
}

std::unordered_map<std::string, std::string> CSVEquityRepository::readAttribute_(const std::string& ticker) const {
    auto path = attributePath_(ticker);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Attributes file not found" + path.string());
    }
    return parseAttributeFile_(path);
}
void CSVEquityRepository::writeCsv_(const std::shared_ptr<const Equity>& asset) const {
    auto path = csvPath_(asset->ticker());
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file for writing: " + path.string());
    }
    file << "ticker,name,denomination,exchange,sector\n";
    file << asset->ticker() + "," + asset->name() + "," + toString(asset->denomination()) + "," + asset->exchange() +
                "," + asset->sector() + "\n";
}

void CSVEquityRepository::writeAttributes_(const std::shared_ptr<const Equity>& asset) const {
    auto path = attributePath_(asset->ticker());
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open attributes file for writing: " + path.string());
    }
    file << "# " + asset->ticker() + ".attr\n";
    for (const auto& [key, value] : asset->attributes()) {
        file << key << "=" << value << "\n";
    }
}

std::shared_ptr<Equity> CSVEquityRepository::parseCsvFile_(const std::filesystem::path& path,
                                                           const std::string& ticker) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + path.string());
    }
    std::string tickerString, name, denomString, exchange, sector;
    Currency denomination;
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        if (std::getline(iss, tickerString, ',') && std::getline(iss, name, ',') &&
            std::getline(iss, denomString, ',') && std::getline(iss, exchange, ',') && std::getline(iss, sector)) {
            denomination = currencyFromString(denomString);
        }
    }
    return std::make_shared<Equity>(Equity(ticker, name, denomination, exchange, sector));
}

std::unordered_map<std::string, std::string> CSVEquityRepository::parseAttributeFile_(
    const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open Attributes file: " + path.string());
    }
    std::unordered_map<std::string, std::string> output;
    std::string key, value;
    std::string line;
    if (std::getline(file, line)) {
        if (!line.empty() && !std::isalpha(static_cast<unsigned char>(line[0])) && line[0] != '#') {
            std::istringstream iss(line);
            if (std::getline(iss, key, '=') && std::getline(iss, value)) {
                output[key] = value;
            }
        }
    }
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            output[key] = value;
        }
    }
    return output;
}

void CSVEquityRepository::save(const std::shared_ptr<const IAsset>& asset) {
    // TODO(JBBLET) look into performance gain from switching to a static_pointer_cast
    auto equity = std::dynamic_pointer_cast<const Equity>(asset);
    if (!equity) {
        throw std::runtime_error("CSVEquityRepository::save called with non-Equity asset");
    }
    writeAttributes_(equity);
    writeCsv_(equity);
}

std::shared_ptr<const IAsset> CSVEquityRepository::load(const std::string& ticker) const {
    std::shared_ptr<Equity> EquityPtr = readCsv_(ticker);
    std::unordered_map<std::string, std::string> attributes;
    try {
        attributes = readAttribute_(ticker);
        EquityPtr->setAttributes(attributes);
    } catch (const std::runtime_error& e) {
    }
    return EquityPtr;
}
bool CSVEquityRepository::exists(const std::string& ticker) const { return std::filesystem::exists(csvPath_(ticker)); }

std::vector<std::string> CSVEquityRepository::listTickers() const {
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

std::unordered_map<std::string, std::shared_ptr<const IAsset>> CSVEquityRepository::loadAll(
    const std::vector<std::string>& tickers) const {
    std::unordered_map<std::string, std::shared_ptr<const IAsset>> output;
    output.reserve(tickers.size());
    for (const auto& ticker : tickers) {
        output[ticker] = load(ticker);
    }
    return output;
}
}  // namespace finance
