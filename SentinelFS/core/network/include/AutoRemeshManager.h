#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>

namespace SentinelFS {

    /**
     * @brief Per-peer health metrics used for auto-remesh decisions.
     */
    struct PeerHealthMetrics {
        std::string peerId;
        int lastRttMs{-1};
        double avgRttMs{-1.0};
        double jitterMs{0.0};
        double packetLossPercent{0.0};
        std::size_t totalProbes{0};
        std::size_t successProbes{0};
        std::chrono::steady_clock::time_point lastUpdated;
    };

    /**
     * @brief Remesh trigger reason
     */
    enum class RemeshTrigger {
        PERIODIC,           // Regular interval check
        QUALITY_DEGRADED,   // Quality fell below threshold
        PEER_DISCONNECTED,  // Peer connection lost
        MANUAL              // User/admin triggered
    };

    /**
     * @brief Auto-remesh decision engine with threshold-based triggers and rate limiting.
     * 
     * Features:
     * - Threshold-based remesh triggers (not just periodic)
     * - Exponential backoff to prevent remesh storms
     * - Transport authentication requirement after remesh
     * - EWMA-filtered metrics for stable decisions
     */
    class AutoRemeshManager {
    public:
        struct Config {
            std::size_t maxActivePeers{8};
            std::size_t minSamplesForDecision{3};
            double lossWeight{10.0};    // weight for packet loss in scoring
            double jitterWeight{0.5};   // weight for jitter in scoring
            
            // Threshold-based triggers
            double rttThresholdMs{300.0};           // RTT above this triggers remesh consideration
            double lossThresholdPercent{5.0};       // Loss above this triggers remesh
            double jitterThresholdMs{50.0};         // Jitter above this triggers remesh
            
            // Rate limiting / backoff
            std::chrono::seconds minRemeshInterval{15};      // Minimum time between remesh
            std::chrono::seconds maxRemeshInterval{300};     // Maximum backoff interval
            double backoffMultiplier{2.0};                   // Exponential backoff factor
            std::size_t maxConsecutiveRemesh{5};             // Max remesh before extended cooldown
            
            // Security
            bool requireReauthOnRemesh{true};       // Require handshake after transport change
        };

        struct PeerInfoSnapshot {
            std::string peerId;
            bool isConnected{false};
            bool isAuthenticated{false};            // Transport-level auth status
        };

        struct RemeshDecision {
            std::vector<std::string> connectPeers;    // should be connected but currently disconnected
            std::vector<std::string> disconnectPeers; // should be disconnected but currently connected
            std::vector<std::string> reauthPeers;     // need re-authentication
            RemeshTrigger trigger{RemeshTrigger::PERIODIC};
            bool shouldExecute{true};                 // false if rate-limited
            std::chrono::milliseconds nextAllowedIn{0}; // time until next remesh allowed
        };

        AutoRemeshManager();
        explicit AutoRemeshManager(const Config& config);

        /**
         * @brief Update metrics for a single RTT probe.
         * @param peerId Peer identifier.
         * @param rttMs Measured RTT in milliseconds (undefined if success == false).
         * @param success Whether the probe succeeded.
         */
        void updateMeasurement(const std::string& peerId, int rttMs, bool success);

        /**
         * @brief Take a snapshot of current metrics for inspection/logging.
         */
        std::vector<PeerHealthMetrics> snapshotMetrics() const;

        /**
         * @brief Compute remesh decision for the given peers.
         * @param peers Known peers with their current connection state.
         * @return Lists of peers to connect/disconnect.
         */
        RemeshDecision computeRemesh(const std::vector<PeerInfoSnapshot>& peers);
        
        /**
         * @brief Check if any peer has degraded quality (threshold-based trigger)
         */
        bool hasQualityDegradation() const;
        
        /**
         * @brief Check if remesh is currently rate-limited
         */
        bool isRateLimited() const;
        
        /**
         * @brief Get time until next remesh is allowed
         */
        std::chrono::milliseconds timeUntilNextRemesh() const;
        
        /**
         * @brief Mark remesh as executed (for rate limiting)
         */
        void markRemeshExecuted();
        
        /**
         * @brief Reset rate limiting (e.g., after successful stable period)
         */
        void resetRateLimiting();
        
        /**
         * @brief Set configuration
         */
        void setConfig(const Config& config);
        Config getConfig() const;

    private:
        struct InternalMetrics {
            int lastRttMs{-1};
            double avgRttMs{-1.0};
            double ewmaRttMs{-1.0};      // EWMA for stable decisions
            double jitterMs{0.0};
            double ewmaJitterMs{0.0};
            std::size_t totalProbes{0};
            std::size_t successProbes{0};
            std::chrono::steady_clock::time_point lastUpdated{};
        };

        double computeScore(const InternalMetrics& m) const;
        bool isMetricDegraded(const InternalMetrics& m) const;
        void updateEwma(InternalMetrics& m, int rttMs);

        Config config_;
        mutable std::mutex mutex_;
        std::unordered_map<std::string, InternalMetrics> metrics_;
        
        // Rate limiting state
        std::chrono::steady_clock::time_point lastRemeshTime_;
        std::chrono::seconds currentBackoff_{15};
        std::size_t consecutiveRemeshCount_{0};
    };

} // namespace SentinelFS
