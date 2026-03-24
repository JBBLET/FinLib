// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/data/implementation/CSVRepository.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"

CSVRepository::CSVRepository(std::filesystem::path directory) : directory_(std::move(directory)) {}

// ITimeSeriesLoader Interface
TimeSeries CSVRepository::load(const std::string& id, int64_t startMs, int64_t endMs) const {
    auto freqs = availableFrequencies(id);
    if (freqs.empty()) {
        throw std::runtime_error("No data found for series: " + id);
    }
    int64_t finestFreq = *std::min_element(freqs.begin(), freqs.end());
    return load(SeriesKey{id, finestFreq}, startMs, endMs);
}

LoaderCapabilities CSVRepository::capabilities(const std::string& id) const {
    auto freqs = availableFrequencies(id);
    if (freqs.empty()) {
        throw std::runtime_error("No data found for series: " + id);
    }
    int64_t finestFreq = *std::min_element(freqs.begin(), freqs.end());
    SeriesKey key{id, finestFreq};
    auto cov = coverage(key);
    int64_t earliest = cov ? cov->coveredFromMs : 0;
    return LoaderCapabilities{earliest, finestFreq};
}

// ITimeSeriesSaver Interface

void CSVRepository::save(const SeriesKey& key, const TimeSeries& ts, const CoverageInfo& cov) {
    writeCsv_(key, ts);
    writeMeta_(cov);
}

void CSVRepository::merge(const SeriesKey& key, const TimeSeries& newData) {
    if (newData.size() == 0) return;

    // Combine existing data (if any) with new data using a sorted map for dedup
    std::map<int64_t, double> combined;

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
    std::vector<int64_t> mergedTs;
    std::vector<double> mergedVals;
    mergedTs.reserve(combined.size());
    mergedVals.reserve(combined.size());
    for (const auto& [t, v] : combined) {
        mergedTs.push_back(t);
        mergedVals.push_back(v);
    }

    TimeSeries merged(key.SeriesId, std::move(mergedTs), std::move(mergedVals));

    int64_t nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    CoverageInfo cov{key, merged.getTimestamps().front(), merged.getTimestamps().back(), "merged", nowMs};

    writeCsv_(key, merged);
    writeMeta_(cov);
}

// ITimeSeriesRepository Interface

bool CSVRepository::exists(const SeriesKey& key) const { return std::filesystem::exists(csvPath_(key)); }

std::optional<CoverageInfo> CSVRepository::coverage(const SeriesKey& key) const {
    if (!std::filesystem::exists(metaPath_(key))) return std::nullopt;
    return readMeta_(key);
}

std::vector<int64_t> CSVRepository::availableFrequencies(const std::string& id) const {
    std::vector<int64_t> freqs;
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
            // Invalid frequency in filename — skip
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

TimeSeries CSVRepository::load(const SeriesKey& key, int64_t startMs, int64_t endMs) const {
    auto ts = readCsvFiltered_(key, startMs, endMs);
    if (ts.size() == 0) {
        throw std::runtime_error("No data found in range for series: " + key.SeriesId);
    }
    return ts;
}

// Helpers
std::filesystem::path CSVRepository::csvPath_(const SeriesKey& key) const {
    return directory_ / key.SeriesId / (std::to_string(key.frequencyInMs) + ".csv");
}

std::filesystem::path CSVRepository::metaPath_(const SeriesKey& key) const {
    return directory_ / key.SeriesId / (std::to_string(key.frequencyInMs) + ".meta");
}

TimeSeries CSVRepository::parseCsvFile_(const std::filesystem::path& path, const std::string& seriesId, int64_t startMs,
                                        int64_t endMs) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + path.string());
    }

    bool applyFilter = (startMs <= endMs);
    std::vector<int64_t> timestamps;
    std::vector<double> values;
    std::string line;

    // Read first line — detect header vs data
    if (std::getline(file, line)) {
        if (!line.empty() && !std::isalpha(static_cast<unsigned char>(line[0])) && line[0] != '#') {
            // First line is data, not a header
            std::istringstream iss(line);
            std::string tsStr;
            std::string valStr;
            if (std::getline(iss, tsStr, ',') && std::getline(iss, valStr, ',')) {
                int64_t ts = std::stoll(tsStr);
                if (!applyFilter || (ts >= startMs && ts <= endMs)) {
                    timestamps.push_back(ts);
                    values.push_back(std::stod(valStr));
                }
            }
        }
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string tsStr;
        std::string valStr;
        if (std::getline(iss, tsStr, ',') && std::getline(iss, valStr, ',')) {
            int64_t ts = std::stoll(tsStr);
            if (!applyFilter || (ts >= startMs && ts <= endMs)) {
                timestamps.push_back(ts);
                values.push_back(std::stod(valStr));
            }
        }
    }

    return TimeSeries(seriesId, std::move(timestamps), std::move(values));
}

TimeSeries CSVRepository::readCsv_(const SeriesKey& key) const {
    auto path = csvPath_(key);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("CSV file not found: " + path.string());
    }
    // startMs > endMs signals "no filter"
    return parseCsvFile_(path, key.SeriesId, 1, 0);
}

TimeSeries CSVRepository::readCsvFiltered_(const SeriesKey& key, int64_t startMs, int64_t endMs) const {
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
    file << std::setprecision(17);
    file << "timestamp,value\n";
    const auto& timestamps = ts.getTimestamps();
    const auto& values = ts.getValues();
    for (size_t i = 0; i < ts.size(); ++i) {
        file << timestamps[i] << "," << values[i] << "\n";
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
