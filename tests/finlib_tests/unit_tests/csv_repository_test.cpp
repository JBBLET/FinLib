// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/implementation/CSVRepository.hpp"

static constexpr int64_t DAILY_MS = 86400000;

class CSVRepositoryTest : public ::testing::Test {
 protected:
    std::filesystem::path testDir;
    std::unique_ptr<CSVRepository> repo;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "finlib_csv_integration_test";
        std::filesystem::remove_all(testDir);
        std::filesystem::create_directories(testDir);
        repo = std::make_unique<CSVRepository>(testDir);
    }

    void TearDown() override { std::filesystem::remove_all(testDir); }

    // Helpers
    static TimeSeries makeSeries(const std::string& id, const std::vector<int64_t>& ts,
                                 const std::vector<double>& vals) {
        return TimeSeries(id, ts, vals);
    }

    static SeriesKey keyFor(const std::string& id, int64_t freq = DAILY_MS) { return SeriesKey{id, freq}; }

    static CoverageInfo coverageFor(const SeriesKey& key, const TimeSeries& ts) {
        if (ts.size() == 0) return CoverageInfo{key, 0, 0, "test", 0};
        return CoverageInfo{key, ts.getTimestamps().front(), ts.getTimestamps().back(), "test", 0};
    }

    std::string readRawFile(const SeriesKey& key) const {
        std::ifstream f(testDir / key.SeriesId / (std::to_string(key.frequencyInMs) + ".csv"));
        return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
};

// ============================================================
// save(key, TimeSeries, CoverageInfo) — full overwrite
// ============================================================

TEST_F(CSVRepositoryTest, SaveFullSeriesCreatesFile) {
    auto ts = makeSeries("save_creates", {1000, 2000}, {1.0, 2.0});
    auto key = keyFor("save_creates");
    repo->save(key, ts, coverageFor(key, ts));
    EXPECT_TRUE(std::filesystem::exists(testDir / "save_creates" / (std::to_string(DAILY_MS) + ".csv")));
}

TEST_F(CSVRepositoryTest, SaveFullSeriesWritesHeader) {
    auto ts = makeSeries("save_header", {1000}, {1.0});
    auto key = keyFor("save_header");
    repo->save(key, ts, coverageFor(key, ts));
    auto raw = readRawFile(key);
    EXPECT_EQ(raw.substr(0, 15), "timestamp,value");
}

TEST_F(CSVRepositoryTest, SaveFullSeriesRoundtrip) {
    std::vector<int64_t> timestamps = {1000, 2000, 3000, 4000, 5000};
    std::vector<double> values = {1.1, 2.2, 3.3, 4.4, 5.5};
    auto ts = makeSeries("roundtrip", timestamps, values);
    auto key = keyFor("roundtrip");
    repo->save(key, ts, coverageFor(key, ts));

    auto loaded = repo->load(key, 0, 999999);

    ASSERT_EQ(loaded.size(), 5);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(loaded.getTimestamps()[i], timestamps[i]);
        EXPECT_NEAR(loaded.getValues()[i], values[i], 1e-9);
    }
}

TEST_F(CSVRepositoryTest, SaveOverwritesExistingFile) {
    auto key = keyFor("overwrite");
    auto ts1 = makeSeries("overwrite", {1000, 2000, 3000}, {1.0, 2.0, 3.0});
    repo->save(key, ts1, coverageFor(key, ts1));

    auto ts2 = makeSeries("overwrite", {9000, 9001}, {9.0, 9.1});
    repo->save(key, ts2, coverageFor(key, ts2));

    auto loaded = repo->load(key, 0, 999999);
    EXPECT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded.getTimestamps()[0], 9000);
}

TEST_F(CSVRepositoryTest, SaveEmptySeriesWritesHeaderOnly) {
    auto ts = makeSeries("empty_series", {}, {});
    auto key = keyFor("empty_series");
    repo->save(key, ts, CoverageInfo{key, 0, 0, "test", 0});

    auto raw = readRawFile(key);
    EXPECT_EQ(raw, "timestamp,value\n");
}

TEST_F(CSVRepositoryTest, SavePreservesNegativeTimestamps) {
    auto ts = makeSeries("negative_ts", {-3000, -2000, -1000}, {-3.0, -2.0, -1.0});
    auto key = keyFor("negative_ts");
    repo->save(key, ts, coverageFor(key, ts));

    auto loaded = repo->load(key, -9999, 0);
    ASSERT_EQ(loaded.size(), 3);
    EXPECT_EQ(loaded.getTimestamps()[0], -3000);
}

TEST_F(CSVRepositoryTest, SavePreservesNegativeValues) {
    auto ts = makeSeries("negative_vals", {1000, 2000}, {-1.5, -2.5});
    auto key = keyFor("negative_vals");
    repo->save(key, ts, coverageFor(key, ts));

    auto loaded = repo->load(key, 0, 999999);
    ASSERT_EQ(loaded.size(), 2);
    EXPECT_NEAR(loaded.getValues()[0], -1.5, 1e-9);
    EXPECT_NEAR(loaded.getValues()[1], -2.5, 1e-9);
}

TEST_F(CSVRepositoryTest, SavePreservesDoubleWithHighPrecision) {
    std::vector<double> vals = {1.0 / 3.0, std::numbers::pi, std::numbers::e};
    auto ts = makeSeries("precision", {1000, 2000, 3000}, vals);
    auto key = keyFor("precision");
    repo->save(key, ts, coverageFor(key, ts));

    auto loaded = repo->load(key, 0, 999999);
    ASSERT_EQ(loaded.size(), 3);
    EXPECT_NEAR(loaded.getValues()[0], 1.0 / 3.0, 1e-6);
    EXPECT_NEAR(loaded.getValues()[1], std::numbers::pi, 1e-6);
    EXPECT_NEAR(loaded.getValues()[2], std::numbers::e, 1e-6);
}

TEST_F(CSVRepositoryTest, SaveCreatesDirectoryIfMissing) {
    auto nestedDir = testDir / "nested" / "subdir";
    CSVRepository nestedRepo(nestedDir);
    auto ts = makeSeries("nested", {1000}, {1.0});
    auto key = keyFor("nested");
    EXPECT_NO_THROW(nestedRepo.save(key, ts, coverageFor(key, ts)));
    EXPECT_TRUE(std::filesystem::exists(nestedDir / "nested" / (std::to_string(DAILY_MS) + ".csv")));
}

// ============================================================
// merge(key, TimeSeries) — combines with existing data
// ============================================================

TEST_F(CSVRepositoryTest, MergeToNewFileCreatesIt) {
    auto key = keyFor("merge_new");
    auto ts = makeSeries("merge_new", {1000, 2000}, {1.0, 2.0});
    repo->merge(key, ts);

    auto loaded = repo->load(key, 0, 999999);
    EXPECT_EQ(loaded.size(), 2);
}

TEST_F(CSVRepositoryTest, MergeAppendsToExistingData) {
    auto key = keyFor("merge_append");
    auto ts1 = makeSeries("merge_append", {1000, 2000}, {1.0, 2.0});
    repo->save(key, ts1, coverageFor(key, ts1));

    auto ts2 = makeSeries("merge_append", {3000, 4000}, {3.0, 4.0});
    repo->merge(key, ts2);

    auto loaded = repo->load(key, 0, 999999);
    ASSERT_EQ(loaded.size(), 4);
    EXPECT_EQ(loaded.getTimestamps()[2], 3000);
    EXPECT_EQ(loaded.getTimestamps()[3], 4000);
}

TEST_F(CSVRepositoryTest, MergeMultipleBatches) {
    auto key = keyFor("multi_merge");
    auto batch1 = makeSeries("multi_merge", {1000, 2000}, {1.0, 2.0});
    auto batch2 = makeSeries("multi_merge", {3000}, {3.0});
    auto batch3 = makeSeries("multi_merge", {4000, 5000}, {4.0, 5.0});

    repo->merge(key, batch1);
    repo->merge(key, batch2);
    repo->merge(key, batch3);

    auto loaded = repo->load(key, 0, 999999);
    EXPECT_EQ(loaded.size(), 5);
    EXPECT_EQ(loaded.getTimestamps()[4], 5000);
}

TEST_F(CSVRepositoryTest, MergeDeduplicatesByTimestamp) {
    auto key = keyFor("merge_dedup");
    auto ts1 = makeSeries("merge_dedup", {1000, 2000, 3000}, {1.0, 2.0, 3.0});
    repo->save(key, ts1, coverageFor(key, ts1));

    // Merge overlapping data — timestamp 2000 has a new value
    auto ts2 = makeSeries("merge_dedup", {2000, 4000}, {22.0, 4.0});
    repo->merge(key, ts2);

    auto loaded = repo->load(key, 0, 999999);
    ASSERT_EQ(loaded.size(), 4);
    EXPECT_EQ(loaded.getTimestamps()[1], 2000);
    EXPECT_NEAR(loaded.getValues()[1], 22.0, 1e-9);  // new value wins
}

TEST_F(CSVRepositoryTest, MergeSortsByTimestamp) {
    auto key = keyFor("merge_sort");
    auto ts1 = makeSeries("merge_sort", {3000, 5000}, {3.0, 5.0});
    repo->save(key, ts1, coverageFor(key, ts1));

    // Merge data that falls between existing timestamps
    auto ts2 = makeSeries("merge_sort", {1000, 4000}, {1.0, 4.0});
    repo->merge(key, ts2);

    auto loaded = repo->load(key, 0, 999999);
    ASSERT_EQ(loaded.size(), 4);
    EXPECT_EQ(loaded.getTimestamps()[0], 1000);
    EXPECT_EQ(loaded.getTimestamps()[1], 3000);
    EXPECT_EQ(loaded.getTimestamps()[2], 4000);
    EXPECT_EQ(loaded.getTimestamps()[3], 5000);
}

TEST_F(CSVRepositoryTest, MergeEmptyDataIsNoOp) {
    auto key = keyFor("merge_empty");
    auto ts = makeSeries("merge_empty", {1000}, {1.0});
    repo->save(key, ts, coverageFor(key, ts));

    auto empty = makeSeries("merge_empty", {}, {});
    EXPECT_NO_THROW(repo->merge(key, empty));

    auto loaded = repo->load(key, 0, 999999);
    EXPECT_EQ(loaded.size(), 1);
}

TEST_F(CSVRepositoryTest, MergeUpdatesCoverage) {
    auto key = keyFor("merge_cov");
    auto ts1 = makeSeries("merge_cov", {2000, 3000}, {2.0, 3.0});
    repo->save(key, ts1, coverageFor(key, ts1));

    auto ts2 = makeSeries("merge_cov", {1000, 5000}, {1.0, 5.0});
    repo->merge(key, ts2);

    auto cov = repo->coverage(key);
    ASSERT_TRUE(cov.has_value());
    EXPECT_EQ(cov->coveredFromMs, 1000);
    EXPECT_EQ(cov->coveredToMs, 5000);
}

// ============================================================
// exists(key) and coverage(key)
// ============================================================

TEST_F(CSVRepositoryTest, ExistsReturnsFalseWhenMissing) { EXPECT_FALSE(repo->exists(keyFor("nonexistent"))); }

TEST_F(CSVRepositoryTest, ExistsReturnsTrueAfterSave) {
    auto key = keyFor("exists_test");
    auto ts = makeSeries("exists_test", {1000}, {1.0});
    repo->save(key, ts, coverageFor(key, ts));
    EXPECT_TRUE(repo->exists(key));
}

TEST_F(CSVRepositoryTest, CoverageReturnsNulloptWhenMissing) {
    EXPECT_FALSE(repo->coverage(keyFor("nonexistent")).has_value());
}

TEST_F(CSVRepositoryTest, CoverageReturnsCorrectBounds) {
    auto key = keyFor("coverage_test");
    auto ts = makeSeries("coverage_test", {1000, 2000, 5000}, {1.0, 2.0, 5.0});
    CoverageInfo expected{key, 1000, 5000, "test", 42};
    repo->save(key, ts, expected);

    auto cov = repo->coverage(key);
    ASSERT_TRUE(cov.has_value());
    EXPECT_EQ(cov->coveredFromMs, 1000);
    EXPECT_EQ(cov->coveredToMs, 5000);
    EXPECT_EQ(cov->source, "test");
    EXPECT_EQ(cov->lastUpdatedMs, 42);
}

// ============================================================
// availableFrequencies(id)
// ============================================================

TEST_F(CSVRepositoryTest, AvailableFrequenciesEmptyForUnknownId) {
    auto freqs = repo->availableFrequencies("unknown");
    EXPECT_TRUE(freqs.empty());
}

TEST_F(CSVRepositoryTest, AvailableFrequenciesReturnsAllStoredFreqs) {
    auto ts = makeSeries("multi_freq", {1000, 2000}, {1.0, 2.0});

    SeriesKey daily{"multi_freq", 86400000};
    SeriesKey hourly{"multi_freq", 3600000};
    repo->save(daily, ts, coverageFor(daily, ts));
    repo->save(hourly, ts, coverageFor(hourly, ts));

    auto freqs = repo->availableFrequencies("multi_freq");
    ASSERT_EQ(freqs.size(), 2);
    std::sort(freqs.begin(), freqs.end());
    EXPECT_EQ(freqs[0], 3600000);
    EXPECT_EQ(freqs[1], 86400000);
}

// ============================================================
// load(id, start, end) — ITimeSeriesLoader overload
// ============================================================

TEST_F(CSVRepositoryTest, LoadByIdFindsFinestFrequency) {
    auto daily = makeSeries("finest_test", {1000, 2000, 3000}, {1.0, 2.0, 3.0});
    auto hourly = makeSeries("finest_test", {1000, 1500, 2000, 2500, 3000}, {1.0, 1.5, 2.0, 2.5, 3.0});

    SeriesKey dailyKey{"finest_test", 86400000};
    SeriesKey hourlyKey{"finest_test", 3600000};
    repo->save(dailyKey, daily, coverageFor(dailyKey, daily));
    repo->save(hourlyKey, hourly, coverageFor(hourlyKey, hourly));

    // load(string, start, end) should pick the finest frequency (hourly)
    auto loaded = repo->load("finest_test", 0, 999999);
    EXPECT_EQ(loaded.size(), 5);
}

TEST_F(CSVRepositoryTest, LoadByIdThrowsForMissingId) {
    EXPECT_THROW(repo->load("nonexistent", 0, 999999), std::runtime_error);
}

TEST_F(CSVRepositoryTest, LoadByIdThrowsWhenRangeMatchesNothing) {
    auto ts = makeSeries("out_of_range", {1000, 2000, 3000}, {1.0, 2.0, 3.0});
    auto key = keyFor("out_of_range");
    repo->save(key, ts, coverageFor(key, ts));

    EXPECT_THROW(repo->load("out_of_range", 5000, 9000), std::runtime_error);
}

// ============================================================
// load(key, start, end) — range-bounded
// ============================================================

TEST_F(CSVRepositoryTest, LoadInclusiveRangeBoundaries) {
    auto ts = makeSeries("inclusive", {1000, 2000, 3000, 4000, 5000}, {1.0, 2.0, 3.0, 4.0, 5.0});
    auto key = keyFor("inclusive");
    repo->save(key, ts, coverageFor(key, ts));

    auto loaded = repo->load(key, 2000, 4000);
    ASSERT_EQ(loaded.size(), 3);
    EXPECT_EQ(loaded.getTimestamps()[0], 2000);
    EXPECT_EQ(loaded.getTimestamps()[2], 4000);
}

TEST_F(CSVRepositoryTest, LoadExactSingleTimestamp) {
    auto ts = makeSeries("single", {1000, 2000, 3000}, {1.0, 2.0, 3.0});
    auto key = keyFor("single");
    repo->save(key, ts, coverageFor(key, ts));

    auto loaded = repo->load(key, 2000, 2000);
    ASSERT_EQ(loaded.size(), 1);
    EXPECT_EQ(loaded.getTimestamps()[0], 2000);
    EXPECT_NEAR(loaded.getValues()[0], 2.0, 1e-9);
}

TEST_F(CSVRepositoryTest, LoadReturnsIdFromSeriesKey) {
    auto ts = makeSeries("original_id", {1000}, {1.0});
    auto key = keyFor("original_id");
    repo->save(key, ts, coverageFor(key, ts));

    auto loaded = repo->load(key, 0, 999999);
    EXPECT_EQ(loaded.getId(), "original_id");
}

TEST_F(CSVRepositoryTest, LoadHandlesFileWithNoHeader) {
    // Write a CSV without a header at the new path
    auto seriesDir = testDir / "no_header";
    std::filesystem::create_directories(seriesDir);
    auto path = seriesDir / (std::to_string(DAILY_MS) + ".csv");
    std::ofstream f(path);
    f << "1000,1.0\n2000,2.0\n3000,3.0\n";
    f.close();

    auto key = keyFor("no_header");
    auto loaded = repo->load(key, 0, 999999);
    ASSERT_EQ(loaded.size(), 3);
    EXPECT_EQ(loaded.getTimestamps()[0], 1000);
}

TEST_F(CSVRepositoryTest, LoadSkipsBlankLines) {
    auto seriesDir = testDir / "blank_lines";
    std::filesystem::create_directories(seriesDir);
    auto path = seriesDir / (std::to_string(DAILY_MS) + ".csv");
    std::ofstream f(path);
    f << "timestamp,value\n";
    f << "1000,1.0\n";
    f << "\n";
    f << "2000,2.0\n";
    f << "\n";
    f << "3000,3.0\n";
    f.close();

    auto key = keyFor("blank_lines");
    auto loaded = repo->load(key, 0, 999999);
    EXPECT_EQ(loaded.size(), 3);
}

// ============================================================
// Interaction between save and merge
// ============================================================

TEST_F(CSVRepositoryTest, SaveThenMergeRoundtrip) {
    auto key = keyFor("mixed");
    std::vector<int64_t> initTs = {1000, 2000, 3000};
    std::vector<double> initVals = {10.0, 20.0, 30.0};
    auto ts = makeSeries("mixed", initTs, initVals);
    repo->save(key, ts, coverageFor(key, ts));

    auto appended = makeSeries("mixed", {4000, 5000}, {40.0, 50.0});
    repo->merge(key, appended);

    auto loaded = repo->load(key, 0, 999999);
    ASSERT_EQ(loaded.size(), 5);
    EXPECT_NEAR(loaded.getValues()[0], 10.0, 1e-9);
    EXPECT_NEAR(loaded.getValues()[4], 50.0, 1e-9);
}

TEST_F(CSVRepositoryTest, SaveAfterMergeResetsFile) {
    auto key = keyFor("reset_test");
    auto merged = makeSeries("reset_test", {1000, 2000}, {1.0, 2.0});
    repo->merge(key, merged);

    auto fresh = makeSeries("reset_test", {9000, 9001}, {9.0, 9.1});
    repo->save(key, fresh, coverageFor(key, fresh));

    auto loaded = repo->load(key, 0, 999999);
    ASSERT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded.getTimestamps()[0], 9000);
}

// ============================================================
// capabilities(id)
// ============================================================

TEST_F(CSVRepositoryTest, CapabilitiesReturnsFinestFrequency) {
    auto daily = makeSeries("caps_test", {1000, 2000}, {1.0, 2.0});
    auto hourly = makeSeries("caps_test", {1000, 1500, 2000}, {1.0, 1.5, 2.0});

    SeriesKey dailyKey{"caps_test", 86400000};
    SeriesKey hourlyKey{"caps_test", 3600000};
    repo->save(dailyKey, daily, CoverageInfo{dailyKey, 1000, 2000, "test", 0});
    repo->save(hourlyKey, hourly, CoverageInfo{hourlyKey, 1000, 2000, "test", 0});

    auto caps = repo->capabilities("caps_test");
    EXPECT_EQ(caps.finestFrequencyMs, 3600000);
    EXPECT_EQ(caps.earliestAvailableMS, 1000);
}

TEST_F(CSVRepositoryTest, CapabilitiesThrowsForUnknownId) {
    EXPECT_THROW(repo->capabilities("nonexistent"), std::runtime_error);
}
