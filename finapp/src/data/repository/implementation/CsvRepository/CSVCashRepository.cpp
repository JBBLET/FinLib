// "Copyright (c) 2026 JBBLET All Rights Reserved."

#include "finapp/data/repository/implementation/CsvRepository/CSVCashRepository.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "csv/csvReader.hpp"
#include "csv/csvReaderAware.hpp"
#include "csv/csvWriter.hpp"
#include "csv/csvWriterAware.hpp"
#include "finapp/common/Error.hpp"
#include "finapp/finance/asset/Cash.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"

using cpputils::csv::CSVReader;
using cpputils::csv::CSVReaderHeaderAware;
using cpputils::csv::CSVWriter;
using cpputils::csv::CSVWriterHeaderAware;
using cpputils::csv::Row;

using finance::Cash;
using finance::Currency;
using finance::currencyFromString;
using finance::IAsset;

namespace finapp {

CSVCashRepository::CSVCashRepository(std::filesystem::path directory) : directory_(std::move(directory)) {
    std::filesystem::create_directories(directory_ / assetTypeToString(assetType_));
}

std::filesystem::path CSVCashRepository::csvPath_(const std::string& ticker) const {
    return directory_ / (assetTypeToString(assetType_)) / (ticker + ".csv");
}

std::shared_ptr<Cash> CSVCashRepository::readCsv_(const std::string& ticker) const {
    auto path = csvPath_(ticker);
    ensure(std::filesystem::exists(path), "CSV file not found: {}", path.string());
    return parseCsvFile_(path, ticker);
}

void CSVCashRepository::writeCsv_(const std::shared_ptr<const Cash>& asset) const {
    auto path = csvPath_(asset->ticker());
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc);
    ensure(file.is_open(), "Cannot open CSV file for writing: {}", path.string());
    CSVWriterHeaderAware writer{CSVWriter{file, ';'}, {"ticker", "denomination"}};
    writer.writeMap(Row{{"ticker", asset->ticker()}, {"denomination", toString(asset->denomination())}}, false);
}

std::shared_ptr<Cash> CSVCashRepository::parseCsvFile_(const std::filesystem::path& path, const std::string& ticker) {
    std::ifstream file(path);
    ensure(file.is_open(), "Cannot open CSV file: {}", path.string());
    CSVReaderHeaderAware reader{CSVReader{file, ';'}};
    for (const auto& row : reader.readAllMaps()) {
        Currency denomination = currencyFromString(row.at("denomination"));
        return std::make_shared<Cash>(Cash(denomination));
    }
    throw Exception("No data found in CSV file: {}", path.string());
}

void CSVCashRepository::save(const std::shared_ptr<const IAsset>& asset) {
    auto cash = std::dynamic_pointer_cast<const Cash>(asset);
    ensure(cash != nullptr, "CSVCashRepository::save called with non-Cash asset");
    writeCsv_(cash);
}

std::shared_ptr<const IAsset> CSVCashRepository::load(const std::string& ticker) const { return readCsv_(ticker); }

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
