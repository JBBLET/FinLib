// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finlib/session/MultiTimeSeriesSession.hpp"

#include <Eigen/Dense>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace analysis {

// ---------------------------------------------------------------------------
// Session management
// ---------------------------------------------------------------------------
void MultiTimeSeriesSession::addSession(std::string name, std::shared_ptr<TimeSeriesSession> session) {
    sessions_[std::move(name)] = std::move(session);
    invalidateCross_();
}

// ---------------------------------------------------------------------------
// Range / frequency
// ---------------------------------------------------------------------------
void MultiTimeSeriesSession::setRange(Timestamp startMs, Timestamp endMs) {
    for (auto& [name, session] : sessions_) session->setRange(startMs, endMs);
    invalidateCross_();
}

void MultiTimeSeriesSession::setFrequency(Timestamp freqMs) {
    for (auto& [name, session] : sessions_) session->setFrequency(freqMs);
    invalidateCross_();
}

// ---------------------------------------------------------------------------
// Per-series access
// ---------------------------------------------------------------------------
TimeSeriesView MultiTimeSeriesSession::seriesView(const std::string& name) const {
    return sessions_.at(name)->sourceView();
}

const TimeSeriesAnalysis& MultiTimeSeriesSession::seriesAnalysis(const std::string& name) {
    return sessions_.at(name)->sourceAnalysis();
}

TimeSeriesView MultiTimeSeriesSession::derivedSeriesView(const std::string& name, const std::string& transform) const {
    return sessions_.at(name)->derivedView(transform);
}

const TimeSeriesAnalysis& MultiTimeSeriesSession::derivedSeriesAnalysis(const std::string& name,
                                                                        const std::string& transform) {
    return sessions_.at(name)->derivedAnalysis(transform);
}

// ---------------------------------------------------------------------------
// Cross-series transforms
// ---------------------------------------------------------------------------
void MultiTimeSeriesSession::addCrossTransform(std::string name, CrossTransform fn) {
    crossTransforms_[name] = std::move(fn);
    crossCaches_.erase(name);
    crossAnalysisCache_.erase(name);
}

TimeSeriesView MultiTimeSeriesSession::crossView(const std::string& name) {
    if (crossCaches_.find(name) == crossCaches_.end()) {
        auto aligned = buildAligned_();
        crossCaches_[name] = std::make_shared<const TimeSeries>(crossTransforms_.at(name)(aligned));
    }
    const auto& ts = crossCaches_.at(name);
    return TimeSeriesView(ts, 0, ts->size());
}

const TimeSeriesAnalysis& MultiTimeSeriesSession::crossAnalysis(const std::string& name) {
    auto& cached = crossAnalysisCache_[name];
    if (!cached.has_value()) cached = TimeSeriesAnalysis(crossView(name));
    return cached.value();
}

// ---------------------------------------------------------------------------
// Matrix analytics
// ---------------------------------------------------------------------------
std::vector<std::vector<double>> MultiTimeSeriesSession::correlationMatrix(const std::vector<std::string>& names,
                                                                           const std::string& transformName) {
    const size_t n = names.size();
    std::vector<Eigen::VectorXd> vecs;
    vecs.reserve(n);
    for (const auto& name : names) {
        const auto& s = sessions_.at(name);
        TimeSeriesView v = transformName.empty() ? s->sourceView() : s->derivedView(transformName);
        vecs.emplace_back(v.asEigenVector());
    }

    std::vector<std::vector<double>> result(n, std::vector<double>(n, 0.0));
    for (size_t i = 0; i < n; ++i) {
        result[i][i] = 1.0;
        for (size_t j = i + 1; j < n; ++j) {
            const auto& a = vecs[i];
            const auto& b = vecs[j];
            if (a.size() != b.size() || a.size() < 2) {
                result[i][j] = result[j][i] = std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            Eigen::VectorXd da = a.array() - a.mean();
            Eigen::VectorXd db = b.array() - b.mean();
            double sa = std::sqrt(da.squaredNorm() / (a.size() - 1));
            double sb = std::sqrt(db.squaredNorm() / (b.size() - 1));
            double corr = (sa > 1e-12 && sb > 1e-12) ? da.dot(db) / ((a.size() - 1) * sa * sb) : 0.0;
            result[i][j] = result[j][i] = corr;
        }
    }
    return result;
}

std::vector<std::vector<double>> MultiTimeSeriesSession::covarianceMatrix(const std::vector<std::string>& names,
                                                                          const std::string& transformName) {
    const size_t n = names.size();
    std::vector<Eigen::VectorXd> vecs;
    vecs.reserve(n);
    for (const auto& name : names) {
        const auto& s = sessions_.at(name);
        TimeSeriesView v = transformName.empty() ? s->sourceView() : s->derivedView(transformName);
        vecs.emplace_back(v.asEigenVector());
    }

    std::vector<std::vector<double>> result(n, std::vector<double>(n, 0.0));
    for (size_t i = 0; i < n; ++i) {
        const auto& a = vecs[i];
        if (a.size() < 2) {
            result[i][i] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        Eigen::VectorXd da = a.array() - a.mean();
        result[i][i] = da.squaredNorm() / (a.size() - 1);
        for (size_t j = i + 1; j < n; ++j) {
            const auto& b = vecs[j];
            if (a.size() != b.size() || b.size() < 2) {
                result[i][j] = result[j][i] = std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            Eigen::VectorXd db = b.array() - b.mean();
            double cov = da.dot(db) / (a.size() - 1);
            result[i][j] = result[j][i] = cov;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
std::vector<std::string> MultiTimeSeriesSession::sessionNames() const {
    std::vector<std::string> names;
    names.reserve(sessions_.size());
    for (const auto& [name, _] : sessions_) names.push_back(name);
    return names;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
std::unordered_map<std::string, TimeSeries> MultiTimeSeriesSession::buildAligned_() const {
    // TODO(JBBLET): Add AlignmentPolicy (Intersection / Union) support for sessions on different grids.
    // For now assumes all sessions share the same timestamp grid.
    std::unordered_map<std::string, TimeSeries> result;
    result.reserve(sessions_.size());
    for (const auto& [name, session] : sessions_) result.emplace(name, session->sourceView().toSeries());
    return result;
}

void MultiTimeSeriesSession::invalidateCross_() {
    crossCaches_.clear();
    crossAnalysisCache_.clear();
}

}  // namespace analysis
