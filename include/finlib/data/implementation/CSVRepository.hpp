// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/interfaces/ITimeSeriesRepository.hpp"

/// Stores time series as CSV files on disk.
///
/// File layout:
///   <directory>/<seriesId>/<frequencyMs>.csv     — timestamp,value rows
///   <directory>/<seriesId>/<frequencyMs>.meta     — key=value coverage metadata
class CSVRepository : public ITimeSeriesRepository {
 public:
    explicit CSVRepository(std::filesystem::path directory);

    // --- ITimeSeriesLoader ---
    /// Finds the finest available frequency for `id` and loads data in [startMs, endMs].
    TimeSeries load(const std::string& id, int64_t startMs, int64_t endMs) const override;
    LoaderCapabilities capabilities(const std::string& id) const override;

    // --- ITimeSeriesSaver ---
    void save(const SeriesKey& key, const TimeSeries& ts, const CoverageInfo& coverage) override;
    void merge(const SeriesKey& key, const TimeSeries& newData) override;

    // --- ITimeSeriesRepository ---
    bool exists(const SeriesKey& key) const override;
    std::optional<CoverageInfo> coverage(const SeriesKey& key) const override;
    std::vector<int64_t> availableFrequencies(const std::string& id) const override;
    TimeSeries load(const SeriesKey& key) const override;
    TimeSeries load(const SeriesKey& key, int64_t startMs, int64_t endMs) const override;

 private:
    std::filesystem::path directory_;

    std::filesystem::path csvPath_(const SeriesKey& key) const;
    std::filesystem::path metaPath_(const SeriesKey& key) const;

    /// Reads all rows from the CSV file. Returns an empty-sized TimeSeries if the file
    /// only contains a header.
    TimeSeries readCsv_(const SeriesKey& key) const;

    /// Reads rows from the CSV file, keeping only those in [startMs, endMs].
    TimeSeries readCsvFiltered_(const SeriesKey& key, int64_t startMs, int64_t endMs) const;

    void writeCsv_(const SeriesKey& key, const TimeSeries& ts) const;

    CoverageInfo readMeta_(const SeriesKey& key) const;
    void writeMeta_(const CoverageInfo& cov) const;

    /// Shared CSV parsing logic. If startMs > endMs, no filtering is applied.
    static TimeSeries parseCsvFile_(const std::filesystem::path& path, const std::string& seriesId,
                                    int64_t startMs, int64_t endMs);
};
