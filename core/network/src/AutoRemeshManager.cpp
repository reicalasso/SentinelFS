#include "AutoRemeshManager.h"
#include "Constants.h"

#include <algorithm>
#include <limits>
#include <cmath>

namespace SentinelFS {

    AutoRemeshManager::AutoRemeshManager()
        : config_() {
    }

    AutoRemeshManager::AutoRemeshManager(const Config& config)
        : config_(config) {
    }

    void AutoRemeshManager::updateMeasurement(const std::string& peerId, int rttMs, bool success) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& m = metrics_[peerId];

        m.totalProbes++;
        m.lastUpdated = std::chrono::steady_clock::now();

        if (!success || rttMs < 0) {
            return;
        }

        m.successProbes++;
        m.lastRttMs = rttMs;

        if (m.successProbes == 1) {
            m.avgRttMs = static_cast<double>(rttMs);
            m.jitterMs = 0.0;
            return;
        }

        double oldAvg = m.avgRttMs < 0.0 ? static_cast<double>(rttMs) : m.avgRttMs;
        double newAvg = oldAvg + (static_cast<double>(rttMs) - oldAvg) / static_cast<double>(m.successProbes);
        double diff = std::abs(static_cast<double>(rttMs) - oldAvg);

        // Exponential moving average for jitter
        const double alpha = 0.1;
        m.jitterMs = (1.0 - alpha) * m.jitterMs + alpha * diff;
        m.avgRttMs = newAvg;
    }

    std::vector<PeerHealthMetrics> AutoRemeshManager::snapshotMetrics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PeerHealthMetrics> out;
        out.reserve(metrics_.size());

        for (const auto& kv : metrics_) {
            const auto& id = kv.first;
            const auto& m = kv.second;

            PeerHealthMetrics h;
            h.peerId = id;
            h.lastRttMs = m.lastRttMs;
            h.avgRttMs = m.avgRttMs;
            h.jitterMs = m.jitterMs;
            h.totalProbes = m.totalProbes;
            h.successProbes = m.successProbes;
            h.lastUpdated = m.lastUpdated;

            if (m.totalProbes > 0) {
                std::size_t failed = m.totalProbes - m.successProbes;
                h.packetLossPercent = 100.0 * static_cast<double>(failed) / static_cast<double>(m.totalProbes);
            } else {
                h.packetLossPercent = 0.0;
            }

            out.push_back(h);
        }

        return out;
    }

    double AutoRemeshManager::computeScore(const InternalMetrics& m) const {
        // Filter out stale metrics
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - m.lastUpdated).count();
        
        if (age > sfs::config::PEER_STALE_TIMEOUT_SEC) {
            return std::numeric_limits<double>::infinity();
        }

        if (m.successProbes < config_.minSamplesForDecision || m.avgRttMs < 0.0) {
            return std::numeric_limits<double>::infinity();
        }

        double lossPercent = 0.0;
        if (m.totalProbes > 0) {
            std::size_t failed = m.totalProbes - m.successProbes;
            lossPercent = 100.0 * static_cast<double>(failed) / static_cast<double>(m.totalProbes);
        }

        double score = m.avgRttMs;
        score += config_.jitterWeight * m.jitterMs;
        score += config_.lossWeight * lossPercent;
        return score;
    }

    AutoRemeshManager::RemeshDecision AutoRemeshManager::computeRemesh(const std::vector<PeerInfoSnapshot>& peers) const {
        RemeshDecision decision;
        if (peers.empty() || config_.maxActivePeers == 0) {
            return decision;
        }

        struct ScoredPeer {
            std::string id;
            bool isConnected;
            double score;
        };

        std::vector<ScoredPeer> scored;
        scored.reserve(peers.size());

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& p : peers) {
                auto it = metrics_.find(p.peerId);
                InternalMetrics m;
                if (it != metrics_.end()) {
                    m = it->second;
                }
                double s = computeScore(m);
                scored.push_back({p.peerId, p.isConnected, s});
            }
        }

        std::sort(scored.begin(), scored.end(), [](const ScoredPeer& a, const ScoredPeer& b) {
            return a.score < b.score;
        });

        std::size_t target = std::min<std::size_t>(config_.maxActivePeers, scored.size());

        // Build desired set of peers (best scores first)
        std::vector<std::string> desired;
        desired.reserve(target);
        for (std::size_t i = 0; i < target; ++i) {
            if (!std::isfinite(scored[i].score)) {
                continue; // skip peers without sufficient metrics
            }
            desired.push_back(scored[i].id);
        }

        // Fallback: if we skipped everyone due to inf scores, just keep current connections
        if (desired.empty()) {
            for (const auto& p : peers) {
                if (p.isConnected) {
                    decision.connectPeers.push_back(p.peerId);
                }
            }
            return decision;
        }

        auto isDesired = [&desired](const std::string& id) {
            return std::find(desired.begin(), desired.end(), id) != desired.end();
        };

        for (const auto& p : peers) {
            bool want = isDesired(p.peerId);
            if (p.isConnected && !want) {
                decision.disconnectPeers.push_back(p.peerId);
            } else if (!p.isConnected && want) {
                decision.connectPeers.push_back(p.peerId);
            }
        }

        return decision;
    }

} // namespace SentinelFS
