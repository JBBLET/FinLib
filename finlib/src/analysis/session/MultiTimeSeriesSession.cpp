// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finlib/analysis/session/MultiTimeSeriesSession.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/logger/PrefixedLogger.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::analysis {

MultiTimeSeriesSession::MultiTimeSeriesSession(logging::ILogger* logger)
    : logger_{logging::PrefixedLogger::wrap(logger, "MultiTimeSeriesSession")} {}

// ---------------------------------------------------------------------------
// Session management
// ---------------------------------------------------------------------------
void MultiTimeSeriesSession::addSession(std::string name, std::shared_ptr<ITimeSeriesSession> session) {
    if (logger_) logger_->write(logging::Level::Debug, "addSession '" + name + "'");
    if (!sessions_.count(name)) sessionNames_.push_back(name);
    invalidate_(name);
    sessions_[std::move(name)] = std::move(session);
}

void MultiTimeSeriesSession::addSession(
    std::unordered_map<std::string, std::shared_ptr<ITimeSeriesSession>> sessionMap) {
    sessions_.reserve(sessions_.size() + sessionMap.size());
    for (auto& [name, session] : sessionMap) addSession(name, std::move(session));
}

// ---------------------------------------------------------------------------
// ITimeSeriesSession
// ---------------------------------------------------------------------------
std::shared_ptr<const TimeSeries> MultiTimeSeriesSession::seriesPtr(const std::string& name) {
    if (name.empty()) throw std::logic_error("MultiTimeSeriesSession has no single source series");
    if (!crossCaches_.count(name)) buildCross_(name);
    return crossCaches_.at(name);
}

TimeSeriesView MultiTimeSeriesSession::seriesView(const std::string& name) {
    if (name.empty()) throw std::logic_error("MultiTimeSeriesSession has no single source series");
    const auto& ts = seriesPtr(name);
    return TimeSeriesView(ts, 0, ts->size());
}

const TimeSeriesAnalysis& MultiTimeSeriesSession::seriesAnalysis(const std::string& name) {
    if (name.empty()) throw std::logic_error("MultiTimeSeriesSession has no single source series");
    auto& cached = crossAnalysisCache_[name];
    if (!cached.has_value()) cached = TimeSeriesAnalysis(seriesView(name));
    return cached.value();
}

// ---------------------------------------------------------------------------
// Range / frequency
// ---------------------------------------------------------------------------
void MultiTimeSeriesSession::setRange(Timestamp startMs, Timestamp endMs) {
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "setRange [" + std::to_string(startMs) + ".." + std::to_string(endMs) + "]");
    for (auto& [name, session] : sessions_) session->setRange(startMs, endMs);
    invalidateAll_();
}

void MultiTimeSeriesSession::setFrequency(Timestamp freqMs) {
    if (logger_) logger_->write(logging::Level::Debug, "setFrequency " + std::to_string(freqMs) + "ms");
    for (auto& [name, session] : sessions_) session->setFrequency(freqMs);
    invalidateAll_();
}

// ---------------------------------------------------------------------------
// Sub-session access
// ---------------------------------------------------------------------------
TimeSeriesView MultiTimeSeriesSession::subSeriesView(const std::string& sessionName, const std::string& seriesName) {
    return sessions_.at(sessionName)->seriesView(seriesName);
}

const TimeSeriesAnalysis& MultiTimeSeriesSession::subSeriesAnalysis(const std::string& sessionName,
                                                                    const std::string& seriesName) {
    return sessions_.at(sessionName)->seriesAnalysis(seriesName);
}

CustomTimeSeriesAnalysis& MultiTimeSeriesSession::customAnalysis(const std::string& seriesName) {
    auto& ca = crossCustomAnalysisCache_[seriesName];
    if (!ca.has_value()) ca = CustomTimeSeriesAnalysis(seriesName, seriesView(seriesName));
    return ca.value();
}

CustomTimeSeriesAnalysis& MultiTimeSeriesSession::subCustomAnalysis(const std::string& sessionName,
                                                                    const std::string& seriesName) {
    return sessions_.at(sessionName)->customAnalysis(seriesName);
}

// ---------------------------------------------------------------------------
// Cross-series transforms
// ---------------------------------------------------------------------------
void MultiTimeSeriesSession::addTransform(std::string name, CrossTransform fn) {
    std::vector<std::string> inputs;
    inputs.reserve(sessions_.size());
    for (const auto& [sessionName, _] : sessions_) inputs.push_back(sessionName);
    addTransform(std::move(name), std::move(inputs), std::move(fn));
}

void MultiTimeSeriesSession::addTransform(std::string name, std::vector<std::string> inputs, CrossTransform fn) {
    addTransform(SeriesNode{std::move(name), std::move(inputs), std::move(fn)});
}

void MultiTimeSeriesSession::addTransform(MultiTimeSeriesSession::SeriesNode node) {
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "addTransform '" + node.name + "' (" + std::to_string(node.inputs.size()) + " inputs)");
    // Remove stale reverse dep entries for this name before re-registering.
    for (auto& [_, deps] : reverseDeps_) deps.erase(std::remove(deps.begin(), deps.end(), node.name), deps.end());

    crossCaches_.erase(node.name);
    crossAnalysisCache_.erase(node.name);

    for (const auto& input : node.inputs) reverseDeps_[input].push_back(node.name);
    crossTransforms_[node.name] = std::move(node);
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
std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> MultiTimeSeriesSession::buildAligned_(
    const std::string& name) const {
    // TODO(JBBLET): Add AlignmentPolicy (Intersection / Union) support for sessions on different grids.
    // For now assumes all sessions share the same timestamp grid.
    const auto& leaf = crossTransforms_.at(name);
    std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> inputsMap;
    inputsMap.reserve(leaf.inputs.size());

    for (const auto& dep : leaf.inputs) {
        const auto sep = dep.find("::");
        if (sep != std::string::npos) {
            // "AAPL::return" → named series from sub-session
            const auto sessionName = dep.substr(0, sep);
            const auto seriesName = dep.substr(sep + 2);
            inputsMap.emplace(dep, sessions_.at(sessionName)->seriesPtr(seriesName));
        } else if (sessions_.count(dep)) {
            // "AAPL" → primary series of sub-session
            inputsMap.emplace(dep, sessions_.at(dep)->seriesPtr(""));
        } else {
            // "nav" → another cross-transform result
            if (!crossCaches_.count(dep)) buildCross_(dep);
            inputsMap.emplace(dep, crossCaches_.at(dep));
        }
    }
    return inputsMap;
}

void MultiTimeSeriesSession::buildCross_(const std::string& name) const {
    if (logger_) logger_->write(logging::Level::Debug, "buildCross_ '" + name + "'");
    auto aligned = buildAligned_(name);
    crossCaches_[name] = std::make_shared<const TimeSeries>(crossTransforms_.at(name).crossTransform(aligned));
}

void MultiTimeSeriesSession::invalidateAll_() {
    crossCaches_.clear();
    crossAnalysisCache_.clear();
    for (auto& [name, ca] : crossCustomAnalysisCache_)
        if (ca.has_value()) ca->rebind(seriesView(name));
}

void MultiTimeSeriesSession::invalidate_(const std::string& name) {
    crossCaches_.erase(name);
    crossAnalysisCache_.erase(name);
    if (auto it = crossCustomAnalysisCache_.find(name); it != crossCustomAnalysisCache_.end())
        if (it->second.has_value()) it->second->rebind(seriesView(name));
    auto it = reverseDeps_.find(name);
    if (it != reverseDeps_.end())
        for (const auto& dep : it->second) invalidate_(dep);
}

}  // namespace ts::analysis
