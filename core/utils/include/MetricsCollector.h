#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>

namespace SentinelFS {

    // Snapshot structs for returning metrics (non-atomic)
    struct SyncMetricsSnapshot {
        uint64_t filesWatched{0};
        uint64_t filesSynced{0};
        uint64_t filesModified{0};
        uint64_t filesDeleted{0};
        uint64_t syncErrors{0};
        uint64_t conflictsDetected{0};
    };

    struct NetworkMetricsSnapshot {
        uint64_t bytesUploaded{0};
        uint64_t bytesDownloaded{0};
        uint64_t peersDiscovered{0};
        uint64_t peersConnected{0};
        uint64_t peersDisconnected{0};
        uint64_t transfersCompleted{0};
        uint64_t transfersFailed{0};
        uint64_t deltasSent{0};
        uint64_t deltasReceived{0};
        uint64_t remeshCycles{0};
    };

    struct SecurityMetricsSnapshot {
        uint64_t anomaliesDetected{0};
        uint64_t suspiciousActivities{0};
        uint64_t syncPausedCount{0};
        uint64_t authFailures{0};
        uint64_t encryptionErrors{0};
    };

    struct PerformanceMetricsSnapshot {
        uint64_t avgSyncLatencyMs{0};
        uint64_t avgDeltaComputeTimeMs{0};
        uint64_t avgTransferSpeedKBps{0};
        uint64_t peakMemoryUsageMB{0};
        uint64_t cpuUsagePercent{0};
        uint64_t avgRemeshRttImprovementMs{0};
    };

    // Internal structs with atomics
    struct SyncMetrics {
        std::atomic<uint64_t> filesWatched{0};
        std::atomic<uint64_t> filesSynced{0};
        std::atomic<uint64_t> filesModified{0};
        std::atomic<uint64_t> filesDeleted{0};
        std::atomic<uint64_t> syncErrors{0};
        std::atomic<uint64_t> conflictsDetected{0};
    };

    struct NetworkMetrics {
        std::atomic<uint64_t> bytesUploaded{0};
        std::atomic<uint64_t> bytesDownloaded{0};
        std::atomic<uint64_t> peersDiscovered{0};
        std::atomic<uint64_t> peersConnected{0};
        std::atomic<uint64_t> peersDisconnected{0};
        std::atomic<uint64_t> transfersCompleted{0};
        std::atomic<uint64_t> transfersFailed{0};
        std::atomic<uint64_t> deltasSent{0};
        std::atomic<uint64_t> deltasReceived{0};
        std::atomic<uint64_t> remeshCycles{0};
    };

    struct SecurityMetrics {
        std::atomic<uint64_t> anomaliesDetected{0};
        std::atomic<uint64_t> suspiciousActivities{0};
        std::atomic<uint64_t> syncPausedCount{0};
        std::atomic<uint64_t> authFailures{0};
        std::atomic<uint64_t> encryptionErrors{0};
    };

    struct PerformanceMetrics {
        std::atomic<uint64_t> avgSyncLatencyMs{0};
        std::atomic<uint64_t> avgDeltaComputeTimeMs{0};
        std::atomic<uint64_t> avgTransferSpeedKBps{0};
        std::atomic<uint64_t> peakMemoryUsageMB{0};
        std::atomic<uint64_t> cpuUsagePercent{0};
        std::atomic<uint64_t> avgRemeshRttImprovementMs{0};
    };

    // Time-series data point for tracking metrics over time
    struct MetricDataPoint {
        std::chrono::system_clock::time_point timestamp;
        uint64_t value;
    };

    class MetricsCollector {
    public:
        static MetricsCollector& instance();

        // Sync metrics
        void incrementFilesWatched();
        void incrementFilesSynced();
        void incrementFilesModified();
        void incrementFilesDeleted();
        void incrementSyncErrors();
        void incrementConflicts();

        // Network metrics
        void addBytesUploaded(uint64_t bytes);
        void addBytesDownloaded(uint64_t bytes);
        void incrementBytesSent(uint64_t bytes) { addBytesUploaded(bytes); }  // Alias
        void incrementBytesReceived(uint64_t bytes) { addBytesDownloaded(bytes); }  // Alias
        void incrementPeersDiscovered();
        void incrementPeersConnected();
        void incrementPeersDisconnected();
        void incrementConnections() { incrementPeersConnected(); }  // Alias
        void incrementDisconnections() { incrementPeersDisconnected(); }  // Alias
        void incrementTransfersCompleted();
        void incrementTransfersFailed();
        void incrementDeltasSent();
        void incrementDeltasReceived();
        void incrementRemeshCycles();

        // Security metrics
        void incrementAnomalies();
        void incrementSuspiciousActivities();
        void incrementSyncPaused();
        void incrementAuthFailures();
        void incrementEncryptionErrors();

        // Performance metrics
        void recordSyncLatency(uint64_t latencyMs);
        void recordDeltaComputeTime(uint64_t timeMs);
        void recordTransferSpeed(uint64_t speedKBps);
        void recordRemeshRttImprovement(uint64_t improvementMs);
        void updateMemoryUsage(uint64_t usageMB);
        void updateCpuUsage(uint64_t percent);

        // Get current metrics (returns snapshots)
        SyncMetricsSnapshot getSyncMetrics() const;
        NetworkMetricsSnapshot getNetworkMetrics() const;
        SecurityMetricsSnapshot getSecurityMetrics() const;
        PerformanceMetricsSnapshot getPerformanceMetrics() const;

        // Get formatted metrics string
        std::string getMetricsSummary() const;
        
        // Prometheus-compatible export
        std::string exportPrometheus() const;

        // Reset all metrics
        void reset();

        // Get uptime
        std::chrono::seconds getUptime() const;

    private:
        MetricsCollector();
        ~MetricsCollector() = default;

        SyncMetrics syncMetrics_;
        NetworkMetrics networkMetrics_;
        SecurityMetrics securityMetrics_;
        PerformanceMetrics perfMetrics_;

        std::chrono::system_clock::time_point startTime_;
        mutable std::mutex mutex_;

        // Helper for moving averages
        void updateMovingAverage(std::atomic<uint64_t>& avg, uint64_t newValue);
    };

} // namespace SentinelFS
