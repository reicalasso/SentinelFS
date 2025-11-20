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
     * @brief Simple auto-remesh decision engine based on RTT, jitter and packet loss.
     */
    class AutoRemeshManager {
    public:
        struct Config {
            std::size_t maxActivePeers{8};
            std::size_t minSamplesForDecision{3};
            double lossWeight{10.0};    // weight for packet loss in scoring
            double jitterWeight{0.5};   // weight for jitter in scoring
        };

        struct PeerInfoSnapshot {
            std::string peerId;
            bool isConnected{false};
        };

        struct RemeshDecision {
            std::vector<std::string> connectPeers;    // should be connected but currently disconnected
            std::vector<std::string> disconnectPeers; // should be disconnected but currently connected
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
        RemeshDecision computeRemesh(const std::vector<PeerInfoSnapshot>& peers) const;

    private:
        struct InternalMetrics {
            int lastRttMs{-1};
            double avgRttMs{-1.0};
            double jitterMs{0.0};
            std::size_t totalProbes{0};
            std::size_t successProbes{0};
            std::chrono::steady_clock::time_point lastUpdated{};
        };

        double computeScore(const InternalMetrics& m) const;

        Config config_;
        mutable std::mutex mutex_;
        std::unordered_map<std::string, InternalMetrics> metrics_;
    };

} // namespace SentinelFS
