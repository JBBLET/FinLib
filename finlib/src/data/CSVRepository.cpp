// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/data/implementation/CSVRepository.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "csv/convert.hpp"
#include "csv/csvReader.hpp"
#include "csv/csvReaderAware.hpp"
#include "csv/csvWriter.hpp"
#include "csv/csvWriterAware.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/logger/PrefixedLogger.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
namespace ts {
CSVRepository::CSVRepository(std::filesystem::path directory, logging::ILogger* logger)
    : directory_(std::move(directory)), logger_(logging::PrefixedLogger::wrap(logger, "CSVRepository")) {
    std::filesystem::create_directories(directory_);
}

// ITimeSeriesLoader Interface
TimeSeries CSVRepository::load(const std::string& id, Timestamp startMs, Timestamp endMs,
                               std::optional<Timestamp> /*requestedFrequency*/) const {
    auto freqs = availableFrequencies(id);
    if (freqs.empty()) {
        throw std::runtime_error("No data found for series: " + id);
    }
    Timestamp finestFreq = *std::min_element(freqs.begin(), freqs.end());
    return load(SeriesKey{id, finestFreq}, startMs, endMs);
}

LoaderCapabilities CSVRepository::capabilities(const std::string& id) const {
    auto freqs = availableFrequencies(id);
    if (freqs.empty()) {
        throw std::runtime_error("No data found for series: " + id);
    }
    Timestamp finestFreq = *std::min_element(freqs.begin(), freqs.end());
    SeriesKey key{id, finestFreq};
    auto cov = coverage(key);
    Timestamp earliest = cov ? cov->coveredFromMs : 0;
    return LoaderCapabilities{earliest, finestFreq};
}

// ITimeSeriesSaver Interface

void CSVRepository::doSave(const SeriesKey& key, const TimeSeries& ts) {
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "CSV write: '" + key.SeriesId + "' freq=" + std::to_string(key.frequencyInMs) + "ms " +
                           std::to_string(ts.size()) + " points -> " + csvPath_(key).string());
    writeCsv_(key, ts);
}

void CSVRepository::doMerge(const SeriesKey& key, const TimeSeries& newData) {
    if (newData.size() == 0) return;

    std::map<Timestamp, double> combined;

    if (exists(key)) {
        auto existing = readCsv_(key);
        const auto& ts = existing.getTimestamps();
        const auto& vals = existing.getValues();
        for (size_t i = 0; i < existing.size(); ++i) {
            combined[ts[i]] = vals[i];
        }
    }

    const auto& newTs = newData.getTimestamps();
    const auto& newVals = newData.getValues();
    for (size_t i = 0; i < newData.size(); ++i) {
        combined[newTs[i]] = newVals[i];
    }

    // Build merged TimeSeries
    Timestamps mergedTs;
    std::vector<double> mergedVals;
    mergedTs.reserve(combined.size());
    mergedVals.reserve(combined.size());
    for (const auto& [t, v] : combined) {
        mergedTs.push_back(t);
        mergedVals.push_back(v);
    }

    TimeSeries merged(key.SeriesId, std::move(mergedTs), std::move(mergedVals));

    if (logger_)
        logger_->write(logging::Level::Debug,
                       "CSV merge: '" + key.SeriesId + "' freq=" + std::to_string(key.frequencyInMs) + "ms +" +
                           std::to_string(newData.size()) + " points -> " + std::to_string(merged.size()) + " total");

    writeCsv_(key, merged);
}

// ITimeSeriesRepository Interface

bool CSVRepository::exists(const SeriesKey& key) const { return std::filesystem::exists(csvPath_(key)); }

std::optional<CoverageInfo> CSVRepository::coverage(const SeriesKey& key) const {
    if (!std::filesystem::exists(csvPath_(key))) return std::nullopt;
    auto ts = readCsv_(key);
    if (ts.size() == 0) return std::nullopt;
    const auto stamps = ts.getTimestamps();
    return CoverageInfo{key, stamps.front(), stamps.back(), "computed", 0};
}

Timestamps CSVRepository::availableFrequencies(const std::string& id) const {
    std::vector<Timestamp> freqs;
    auto seriesDir = directory_ / id;
    if (!std::filesystem::exists(seriesDir) || !std::filesystem::is_directory(seriesDir)) {
        return freqs;
    }

    for (const auto& entry : std::filesystem::directory_iterator(seriesDir)) {
        if (!entry.is_regular_file()) continue;
        auto filename = entry.path().filename().string();
        if (!filename.ends_with(".csv")) continue;

        std::string stem = filename.substr(0, filename.size() - 4);
        try {
            freqs.push_back(std::stoll(stem));
        } catch (...) {
        }
    }
    return freqs;
}

TimeSeries CSVRepository::load(const SeriesKey& key) const {
    auto ts = readCsv_(key);
    if (ts.size() == 0) {
        throw std::runtime_error("No data found for series: " + key.SeriesId);
    }
    return ts;
}

TimeSeries CSVRepository::load(const SeriesKey& key, Timestamp startMs, Timestamp endMs) const {
    auto ts = readCsvFiltered_(key, startMs, endMs);
    if (ts.size() == 0) {
        throw std::runtime_error("No data found in range for series: " + key.SeriesId);
    }
    return ts;
}

std::filesystem::path CSVRepository::csvPath_(const SeriesKey& key) const {
    return directory_ / key.SeriesId / (std::to_string(key.frequencyInMs) + ".csv");
}

std::filesystem::path CSVRepository::metaPath_(const SeriesKey& key) const {
    return directory_ / key.SeriesId / (std::to_string(key.frequencyInMs) + ".meta");
}

TimeSeries CSVRepository::parseCsvFile_(const std::filesystem::path& path, const std::string& seriesId,
                                        Timestamp startMs, Timestamp endMs) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + path.string());
    }

    const bool applyFilter = (startMs <= endMs);
    Timestamps timestamps;
    std::vector<double> values;

    CSVReaderHeaderAware reader{CSVReader{file, ';'}};
    for (const auto& row : reader.readAllMaps()) {
        const Timestamp ts = csv::convert::parseInt<Timestamp>(row.at("timestamp"));
        if (applyFilter && (ts < startMs || ts > endMs)) continue;
        const double v = csv::convert::parseFloat<double>(row.at("value"));
        if (std::isnan(v)) continue;
        timestamps.push_back(ts);
        values.push_back(v);
    }

    return TimeSeries(seriesId, std::move(timestamps), std::move(values));
}

TimeSeries CSVRepository::readCsv_(const SeriesKey& key) const {
    auto path = csvPath_(key);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("CSV file not found: " + path.string());
    }
    return parseCsvFile_(path, key.SeriesId, 1, 0);
}

TimeSeries CSVRepository::readCsvFiltered_(const SeriesKey& key, Timestamp startMs, Timestamp endMs) const {
    auto path = csvPath_(key);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("CSV file not found: " + path.string());
    }
    return parseCsvFile_(path, key.SeriesId, startMs, endMs);
}

void CSVRepository::writeCsv_(const SeriesKey& key, const TimeSeries& ts) const {
    auto path = csvPath_(key);
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file for writing: " + path.string());
    }
    CSVWriterHeaderAware writer{CSVWriter{file, ';'}, {"timestamp", "value"}};
    const auto& timestamps = ts.getTimestamps();
    const auto& values = ts.getValues();
    for (size_t i = 0; i < ts.size(); ++i) {
        writer.writeMap(
            Row{{"timestamp", std::to_string(timestamps[i])}, {"value", std::format("{}", values[i])}}, false);
    }
}

CoverageInfo CSVRepository::readMeta_(const SeriesKey& key) const {
    auto path = metaPath_(key);
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open meta file: " + path.string());
    }

    CoverageInfo info{key, 0, 0, "", 0};
    std::string line;
    while (std::getline(file, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        if (k == "coveredFromMs")
            info.coveredFromMs = std::stoll(v);
        else if (k == "coveredToMs")
            info.coveredToMs = std::stoll(v);
        else if (k == "source")
            info.source = v;
        else if (k == "lastUpdatedMs")
            info.lastUpdatedMs = std::stoll(v);
    }
    return info;
}

void CSVRepository::writeMeta_(const CoverageInfo& cov) const {
    auto path = metaPath_(cov.key);
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open meta file for writing: " + path.string());
    }
    file << "coveredFromMs=" << cov.coveredFromMs << "\n";
    file << "coveredToMs=" << cov.coveredToMs << "\n";
    file << "source=" << cov.source << "\n";
    file << "lastUpdatedMs=" << cov.lastUpdatedMs << "\n";
}
}  // namespace ts
