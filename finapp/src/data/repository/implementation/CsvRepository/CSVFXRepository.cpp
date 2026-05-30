// "Copyright (c) 2026 JBBLET All Rights Reserved."

#include "finapp/data/repository/implementation/CsvRepository/CSVFXRepository.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "finapp/finance/common/Currency.hpp"

namespace finapp {

using namespace finance;

CSVFXRepository::CSVFXRepository(std::filesystem::path directory) : directory_(std::move(directory)) {
    std::filesystem::create_directories(directory_);
}

std::filesystem::path CSVFXRepository::csvPath_() const { return directory_ / "fx_pairs.csv"; }

std::vector<FXInfos> CSVFXRepository::readAll_() const {
    auto path = csvPath_();
    if (!std::filesystem::exists(path)) {
        return {};
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open FX CSV file: " + path.string());
    }
    std::vector<FXInfos> entries;
    std::string line;
    std::getline(file, line);  // skip header
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string baseStr, quoteStr, seriesId;
        if (std::getline(iss, baseStr, ',') && std::getline(iss, quoteStr, ',') && std::getline(iss, seriesId)) {
            entries.push_back(
                FXInfos{currencyFromString(baseStr), currencyFromString(quoteStr), seriesId});
        }
    }
    return entries;
}

void CSVFXRepository::writeAll_(const std::vector<FXInfos>& entries) const {
    auto path = csvPath_();
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open FX CSV file for writing: " + path.string());
    }
    file << "baseCurrency,quoteCurrency,timeseriesID\n";
    for (const auto& entry : entries) {
        file << toString(entry.baseCurrency) + "," + toString(entry.quoteCurrency) + "," + entry.timeseriesID + "\n";
    }
}

FXInfos CSVFXRepository::load(const Currency& baseCurrency, const Currency& quoteCurrency) const {
    auto entries = readAll_();
    for (const auto& entry : entries) {
        if (entry.baseCurrency == baseCurrency && entry.quoteCurrency == quoteCurrency) {
            return entry;
        }
    }
    throw std::runtime_error("FX pair not found: " + toString(baseCurrency) + "/" + toString(quoteCurrency));
}

void CSVFXRepository::save(const FXInfos& fxInfos) {
    auto entries = readAll_();
    bool found = false;
    for (auto& entry : entries) {
        if (entry.baseCurrency == fxInfos.baseCurrency && entry.quoteCurrency == fxInfos.quoteCurrency) {
            entry.timeseriesID = fxInfos.timeseriesID;
            found = true;
            break;
        }
    }
    if (!found) {
        entries.push_back(fxInfos);
    }
    writeAll_(entries);
}

bool CSVFXRepository::exists(const Currency& baseCurrency, const Currency& quoteCurrency) const {
    auto entries = readAll_();
    for (const auto& entry : entries) {
        if (entry.baseCurrency == baseCurrency && entry.quoteCurrency == quoteCurrency) {
            return true;
        }
    }
    return false;
}
}  // namespace finapp
