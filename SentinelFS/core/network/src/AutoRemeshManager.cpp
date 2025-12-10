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
            m.ewmaRttMs = static_cast<double>(rttMs);
            m.jitterMs = 0.0;
            m.ewmaJitterMs = 0.0;
            return;
        }

        double oldAvg = m.avgRttMs < 0.0 ? static_cast<double>(rttMs) : m.avgRttMs;
        double newAvg = oldAvg + (static_cast<double>(rttMs) - oldAvg) / static_cast<double>(m.successProbes);
        double diff = std::abs(static_cast<double>(rttMs) - oldAvg);

        // Exponential moving average for jitter
        const double alpha = 0.2;  // EWMA alpha
        m.jitterMs = (1.0 - alpha) * m.jitterMs + alpha * diff;
        m.avgRttMs = newAvg;
        
        // Update EWMA values for stable decisions
        updateEwma(m, rttMs);
    }
    
    void AutoRemeshManager::updateEwma(InternalMetrics& m, int rttMs) {
        const double alpha = 0.2;
        
        if (m.ewmaRttMs < 0) {
            m.ewmaRttMs = static_cast<double>(rttMs);
            m.ewmaJitterMs = m.jitterMs;
        } else {
            m.ewmaRttMs = alpha * rttMs + (1.0 - alpha) * m.ewmaRttMs;
            m.ewmaJitterMs = alpha * m.jitterMs + (1.0 - alpha) * m.ewmaJitterMs;
        }
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

        // Use EWMA values for more stable scoring
        double rtt = m.ewmaRttMs > 0 ? m.ewmaRttMs : m.avgRttMs;
        double jitter = m.ewmaJitterMs > 0 ? m.ewmaJitterMs : m.jitterMs;
        
        double score = rtt;
        score += config_.jitterWeight * jitter;
        score += config_.lossWeight * lossPercent;
        return score;
    }
    
    bool AutoRemeshManager::isMetricDegraded(const InternalMetrics& m) const {
        if (m.successProbes < config_.minSamplesForDecision) {
            return false;  // Not enough data
        }
        
        double rtt = m.ewmaRttMs > 0 ? m.ewmaRttMs : m.avgRttMs;
        double jitter = m.ewmaJitterMs > 0 ? m.ewmaJitterMs : m.jitterMs;
        
        double lossPercent = 0.0;
        if (m.totalProbes > 0) {
            std::size_t failed = m.totalProbes - m.successProbes;
            lossPercent = 100.0 * static_cast<double>(failed) / static_cast<double>(m.totalProbes);
        }
        
        return rtt > config_.rttThresholdMs ||
               jitter > config_.jitterThresholdMs ||
               lossPercent > config_.lossThresholdPercent;
    }
    
    bool AutoRemeshManager::hasQualityDegradation() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [peerId, m] : metrics_) {
            if (isMetricDegraded(m)) {
                return true;
            }
        }
        return false;
    }
    
    bool AutoRemeshManager::isRateLimited() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (lastRemeshTime_ == std::chrono::steady_clock::time_point{}) {
            return false;  // Never executed
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastRemeshTime_);
        
        return elapsed < currentBackoff_;
    }
    
    std::chrono::milliseconds AutoRemeshManager::timeUntilNextRemesh() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (lastRemeshTime_ == std::chrono::steady_clock::time_point{}) {
            return std::chrono::milliseconds{0};
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRemeshTime_);
        auto backoffMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentBackoff_);
        
        if (elapsed >= backoffMs) {
            return std::chrono::milliseconds{0};
        }
        
        return backoffMs - elapsed;
    }
    
    void AutoRemeshManager::markRemeshExecuted() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        lastRemeshTime_ = std::chrono::steady_clock::now();
        consecutiveRemeshCount_++;
        
        // Exponential backoff
        if (consecutiveRemeshCount_ >= config_.maxConsecutiveRemesh) {
            currentBackoff_ = config_.maxRemeshInterval;
        } else {
            auto newBackoff = std::chrono::seconds(
                static_cast<long long>(currentBackoff_.count() * config_.backoffMultiplier));
            
            if (newBackoff > config_.maxRemeshInterval) {
                newBackoff = config_.maxRemeshInterval;
            }
            currentBackoff_ = newBackoff;
        }
    }
    
    void AutoRemeshManager::resetRateLimiting() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        currentBackoff_ = config_.minRemeshInterval;
        consecutiveRemeshCount_ = 0;
    }
    
    void AutoRemeshManager::setConfig(const Config& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }
    
    AutoRemeshManager::Config AutoRemeshManager::getConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    AutoRemeshManager::RemeshDecision AutoRemeshManager::computeRemesh(const std::vector<PeerInfoSnapshot>& peers) {
        RemeshDecision decision;
        if (peers.empty() || config_.maxActivePeers == 0) {
            decision.shouldExecute = false;
            return decision;
        }
        
        // Check rate limiting
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (lastRemeshTime_ != std::chrono::steady_clock::time_point{}) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastRemeshTime_);
                
                if (elapsed < currentBackoff_) {
                    decision.shouldExecute = false;
                    decision.nextAllowedIn = std::chrono::duration_cast<std::chrono::milliseconds>(
                        currentBackoff_ - elapsed);
                    return decision;
                }
            }
        }

        struct ScoredPeer {
            std::string id;
            bool isConnected;
            bool isAuthenticated;
            double score;
            bool isDegraded;
        };

        std::vector<ScoredPeer> scored;
        scored.reserve(peers.size());
        bool anyDegraded = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& p : peers) {
                auto it = metrics_.find(p.peerId);
                InternalMetrics m;
                bool degraded = false;
                if (it != metrics_.end()) {
                    m = it->second;
                    degraded = isMetricDegraded(m);
                    if (degraded) anyDegraded = true;
                }
                double s = computeScore(m);
                scored.push_back({p.peerId, p.isConnected, p.isAuthenticated, s, degraded});
            }
        }
        
        // Determine trigger reason
        if (anyDegraded) {
            decision.trigger = RemeshTrigger::QUALITY_DEGRADED;
        } else {
            decision.trigger = RemeshTrigger::PERIODIC;
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
            decision.shouldExecute = false;  // No changes needed
            return decision;
        }

        auto isDesired = [&desired](const std::string& id) {
            return std::find(desired.begin(), desired.end(), id) != desired.end();
        };

        for (const auto& p : scored) {
            bool want = isDesired(p.id);
            if (p.isConnected && !want) {
                decision.disconnectPeers.push_back(p.id);
            } else if (!p.isConnected && want) {
                decision.connectPeers.push_back(p.id);
            }
            
            // Check if re-authentication is needed
            if (config_.requireReauthOnRemesh && p.isConnected && !p.isAuthenticated) {
                decision.reauthPeers.push_back(p.id);
            }
        }
        
        // Only execute if there are actual changes
        decision.shouldExecute = !decision.connectPeers.empty() || 
                                  !decision.disconnectPeers.empty() ||
                                  !decision.reauthPeers.empty();

        return decision;
    }

} // namespace SentinelFS
