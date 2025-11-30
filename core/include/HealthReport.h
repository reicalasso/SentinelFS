#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace SentinelFS {

/**
 * @brief Anomaly detection report for UI consumption
 */
struct AnomalyReport {
    double score{0.0};           // 0.0 = normal, 1.0 = critical
    std::string lastType;        // e.g., "RAPID_MODIFICATIONS", "RAPID_DELETIONS"
    int64_t lastDetectedAt{0};   // Unix timestamp
};

/**
 * @brief Per-peer health report for UI consumption
 */
struct PeerHealthReport {
    std::string peerId;
    double avgRttMs{-1.0};
    double jitterMs{0.0};
    double packetLossPercent{0.0};
    bool degraded{false};        // true if jitter or loss exceeds thresholds
};

/**
 * @brief System health summary for dashboard
 */
struct HealthSummary {
    // Disk usage
    uint64_t diskTotalBytes{0};
    uint64_t diskFreeBytes{0};
    double diskUsagePercent{0.0};
    
    // Database status
    bool dbConnected{false};
    int64_t dbSizeBytes{0};
    
    // Watcher status
    int activeWatcherCount{0};
    
    // Overall health
    bool healthy{true};
    std::string statusMessage;
};

/**
 * @brief Thresholds for peer health degradation
 */
struct HealthThresholds {
    double jitterThresholdMs{50.0};      // Jitter above this = degraded
    double packetLossThresholdPercent{5.0}; // Loss above this = degraded
    double rttThresholdMs{500.0};        // RTT above this = degraded
};

} // namespace SentinelFS
