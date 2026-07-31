// "Copyright (c) 2026 JBBLET All Rights Reserved."

#include "finapp/data/repository/implementation/CsvRepository/CSVFXRepository.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "csv/csvReader.hpp"
#include "csv/csvReaderAware.hpp"
#include "csv/csvWriter.hpp"
#include "csv/csvWriterAware.hpp"
#include "finapp/common/Error.hpp"
#include "finapp/finance/common/Currency.hpp"

using cpputils::csv::CSVReader;
using cpputils::csv::CSVReaderHeaderAware;
using cpputils::csv::CSVWriter;
using cpputils::csv::CSVWriterHeaderAware;
using cpputils::csv::Row;

using finance::Currency;
using finance::currencyFromString;

namespace finapp {

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
    ensure(file.is_open(), "Cannot open FX CSV file: {}.", path.string());
    std::vector<FXInfos> entries;
    CSVReaderHeaderAware reader{CSVReader{file, ';'}};
    for (const auto& row : reader.readAllMaps()) {
        entries.push_back(FXInfos{currencyFromString(row.at("baseCurrency")),
                                  currencyFromString(row.at("quoteCurrency")),
                                  row.at("timeseriesID")});
    }
    return entries;
}

void CSVFXRepository::writeAll_(const std::vector<FXInfos>& entries) const {
    auto path = csvPath_();
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc);
    ensure(file.is_open(), "Cannot open FX CSV file for writing: {}", path.string());
    CSVWriterHeaderAware writer{CSVWriter{file, ';'}, {"baseCurrency", "quoteCurrency", "timeseriesID"}};
    for (const auto& entry : entries) {
        writer.writeMap(Row{{"baseCurrency", toString(entry.baseCurrency)},
                            {"quoteCurrency", toString(entry.quoteCurrency)},
                            {"timeseriesID", entry.timeseriesID}},
                        false);
    }
}

FXInfos CSVFXRepository::load(const Currency& baseCurrency, const Currency& quoteCurrency) const {
    auto entries = readAll_();
    for (const auto& entry : entries) {
        if (entry.baseCurrency == baseCurrency && entry.quoteCurrency == quoteCurrency) {
            return entry;
        }
    }
    throw Exception("FX pair not found: {}/{}.", toString(baseCurrency), toString(quoteCurrency));
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
