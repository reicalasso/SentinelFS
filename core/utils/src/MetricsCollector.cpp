#include "MetricsCollector.h"
#include <sstream>
#include <iomanip>

namespace SentinelFS {

    MetricsCollector::MetricsCollector() 
        : startTime_(std::chrono::system_clock::now()) {
    }

    MetricsCollector& MetricsCollector::instance() {
        static MetricsCollector instance;
        return instance;
    }

    // Sync metrics
    void MetricsCollector::incrementFilesWatched() { syncMetrics_.filesWatched++; }
    void MetricsCollector::incrementFilesSynced() { syncMetrics_.filesSynced++; }
    void MetricsCollector::incrementFilesModified() { syncMetrics_.filesModified++; }
    void MetricsCollector::incrementFilesDeleted() { syncMetrics_.filesDeleted++; }
    void MetricsCollector::incrementSyncErrors() { syncMetrics_.syncErrors++; }
    void MetricsCollector::incrementConflicts() { syncMetrics_.conflictsDetected++; }

    // Network metrics
    void MetricsCollector::addBytesUploaded(uint64_t bytes) { 
        networkMetrics_.bytesUploaded += bytes; 
    }
    
    void MetricsCollector::addBytesDownloaded(uint64_t bytes) { 
        networkMetrics_.bytesDownloaded += bytes; 
    }
    
    void MetricsCollector::incrementPeersDiscovered() { networkMetrics_.peersDiscovered++; }
    void MetricsCollector::incrementPeersConnected() { networkMetrics_.peersConnected++; }
    void MetricsCollector::incrementPeersDisconnected() { networkMetrics_.peersDisconnected++; }
    void MetricsCollector::incrementTransfersCompleted() { networkMetrics_.transfersCompleted++; }
    void MetricsCollector::incrementTransfersFailed() { networkMetrics_.transfersFailed++; }
    void MetricsCollector::incrementDeltasSent() { networkMetrics_.deltasSent++; }
    void MetricsCollector::incrementDeltasReceived() { networkMetrics_.deltasReceived++; }
    void MetricsCollector::incrementRemeshCycles() { networkMetrics_.remeshCycles++; }

    // Security metrics
    void MetricsCollector::incrementAnomalies() { securityMetrics_.anomaliesDetected++; }
    void MetricsCollector::incrementSuspiciousActivities() { securityMetrics_.suspiciousActivities++; }
    void MetricsCollector::incrementSyncPaused() { securityMetrics_.syncPausedCount++; }
    void MetricsCollector::incrementAuthFailures() { securityMetrics_.authFailures++; }
    void MetricsCollector::incrementEncryptionErrors() { securityMetrics_.encryptionErrors++; }

    // Performance metrics
    void MetricsCollector::recordSyncLatency(uint64_t latencyMs) {
        updateMovingAverage(perfMetrics_.avgSyncLatencyMs, latencyMs);
    }

    void MetricsCollector::recordDeltaComputeTime(uint64_t timeMs) {
        updateMovingAverage(perfMetrics_.avgDeltaComputeTimeMs, timeMs);
    }

    void MetricsCollector::recordTransferSpeed(uint64_t speedKBps) {
        updateMovingAverage(perfMetrics_.avgTransferSpeedKBps, speedKBps);
    }

    void MetricsCollector::recordRemeshRttImprovement(uint64_t improvementMs) {
        updateMovingAverage(perfMetrics_.avgRemeshRttImprovementMs, improvementMs);
    }

    void MetricsCollector::updateMemoryUsage(uint64_t usageMB) {
        uint64_t current = perfMetrics_.peakMemoryUsageMB.load();
        if (usageMB > current) {
            perfMetrics_.peakMemoryUsageMB = usageMB;
        }
    }

    void MetricsCollector::updateCpuUsage(uint64_t percent) {
        perfMetrics_.cpuUsagePercent = percent;
    }

    // Get metrics - create snapshots
    SyncMetricsSnapshot MetricsCollector::getSyncMetrics() const {
        SyncMetricsSnapshot snapshot;
        snapshot.filesWatched = syncMetrics_.filesWatched.load();
        snapshot.filesSynced = syncMetrics_.filesSynced.load();
        snapshot.filesModified = syncMetrics_.filesModified.load();
        snapshot.filesDeleted = syncMetrics_.filesDeleted.load();
        snapshot.syncErrors = syncMetrics_.syncErrors.load();
        snapshot.conflictsDetected = syncMetrics_.conflictsDetected.load();
        return snapshot;
    }

    NetworkMetricsSnapshot MetricsCollector::getNetworkMetrics() const {
        NetworkMetricsSnapshot snapshot;
        snapshot.bytesUploaded = networkMetrics_.bytesUploaded.load();
        snapshot.bytesDownloaded = networkMetrics_.bytesDownloaded.load();
        snapshot.peersDiscovered = networkMetrics_.peersDiscovered.load();
        snapshot.peersConnected = networkMetrics_.peersConnected.load();
        snapshot.peersDisconnected = networkMetrics_.peersDisconnected.load();
        snapshot.transfersCompleted = networkMetrics_.transfersCompleted.load();
        snapshot.transfersFailed = networkMetrics_.transfersFailed.load();
        snapshot.deltasSent = networkMetrics_.deltasSent.load();
        snapshot.deltasReceived = networkMetrics_.deltasReceived.load();
        snapshot.remeshCycles = networkMetrics_.remeshCycles.load();
        return snapshot;
    }

    SecurityMetricsSnapshot MetricsCollector::getSecurityMetrics() const {
        SecurityMetricsSnapshot snapshot;
        snapshot.anomaliesDetected = securityMetrics_.anomaliesDetected.load();
        snapshot.suspiciousActivities = securityMetrics_.suspiciousActivities.load();
        snapshot.syncPausedCount = securityMetrics_.syncPausedCount.load();
        snapshot.authFailures = securityMetrics_.authFailures.load();
        snapshot.encryptionErrors = securityMetrics_.encryptionErrors.load();
        return snapshot;
    }

    PerformanceMetricsSnapshot MetricsCollector::getPerformanceMetrics() const {
        PerformanceMetricsSnapshot snapshot;
        snapshot.avgSyncLatencyMs = perfMetrics_.avgSyncLatencyMs.load();
        snapshot.avgDeltaComputeTimeMs = perfMetrics_.avgDeltaComputeTimeMs.load();
        snapshot.avgTransferSpeedKBps = perfMetrics_.avgTransferSpeedKBps.load();
        snapshot.peakMemoryUsageMB = perfMetrics_.peakMemoryUsageMB.load();
        snapshot.cpuUsagePercent = perfMetrics_.cpuUsagePercent.load();
        snapshot.avgRemeshRttImprovementMs = perfMetrics_.avgRemeshRttImprovementMs.load();
        return snapshot;
    }

    std::string MetricsCollector::getMetricsSummary() const {
        std::stringstream ss;
        
        auto uptime = getUptime();
        auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime).count();
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime % std::chrono::hours(1)).count();
        
        ss << "=== SentinelFS Metrics Summary ===" << std::endl;
        ss << "Uptime: " << hours << "h " << minutes << "m" << std::endl << std::endl;
        
        ss << "--- Sync Metrics ---" << std::endl;
        ss << "  Files Watched: " << syncMetrics_.filesWatched.load() << std::endl;
        ss << "  Files Synced: " << syncMetrics_.filesSynced.load() << std::endl;
        ss << "  Files Modified: " << syncMetrics_.filesModified.load() << std::endl;
        ss << "  Files Deleted: " << syncMetrics_.filesDeleted.load() << std::endl;
        ss << "  Sync Errors: " << syncMetrics_.syncErrors.load() << std::endl;
        ss << "  Conflicts: " << syncMetrics_.conflictsDetected.load() << std::endl << std::endl;
        
        ss << "--- Network Metrics ---" << std::endl;
        double uploadMB = networkMetrics_.bytesUploaded.load() / (1024.0 * 1024.0);
        double downloadMB = networkMetrics_.bytesDownloaded.load() / (1024.0 * 1024.0);
        ss << std::fixed << std::setprecision(2);
        ss << "  Uploaded: " << uploadMB << " MB" << std::endl;
        ss << "  Downloaded: " << downloadMB << " MB" << std::endl;
        ss << "  Peers Discovered: " << networkMetrics_.peersDiscovered.load() << std::endl;
        ss << "  Peers Connected: " << networkMetrics_.peersConnected.load() << std::endl;
        ss << "  Transfers Completed: " << networkMetrics_.transfersCompleted.load() << std::endl;
        ss << "  Transfers Failed: " << networkMetrics_.transfersFailed.load() << std::endl;
        ss << "  Deltas Sent: " << networkMetrics_.deltasSent.load() << std::endl;
        ss << "  Deltas Received: " << networkMetrics_.deltasReceived.load() << std::endl << std::endl;
        
        ss << "--- Security Metrics ---" << std::endl;
        ss << "  Anomalies Detected: " << securityMetrics_.anomaliesDetected.load() << std::endl;
        ss << "  Suspicious Activities: " << securityMetrics_.suspiciousActivities.load() << std::endl;
        ss << "  Sync Paused: " << securityMetrics_.syncPausedCount.load() << " times" << std::endl;
        ss << "  Auth Failures: " << securityMetrics_.authFailures.load() << std::endl;
        ss << "  Encryption Errors: " << securityMetrics_.encryptionErrors.load() << std::endl << std::endl;
        
        ss << "--- Performance Metrics ---" << std::endl;
        ss << "  Avg Sync Latency: " << perfMetrics_.avgSyncLatencyMs.load() << " ms" << std::endl;
        ss << "  Avg Delta Compute: " << perfMetrics_.avgDeltaComputeTimeMs.load() << " ms" << std::endl;
        ss << "  Avg Transfer Speed: " << perfMetrics_.avgTransferSpeedKBps.load() << " KB/s" << std::endl;
        ss << "  Peak Memory Usage: " << perfMetrics_.peakMemoryUsageMB.load() << " MB" << std::endl;
        ss << "  CPU Usage: " << perfMetrics_.cpuUsagePercent.load() << "%" << std::endl;
        
        return ss.str();
    }

    std::string MetricsCollector::exportPrometheus() const {
        std::stringstream ss;
        
        // Sync metrics
        ss << "# HELP sentinelfs_files_watched Total number of files being watched" << std::endl;
        ss << "# TYPE sentinelfs_files_watched counter" << std::endl;
        ss << "sentinelfs_files_watched " << syncMetrics_.filesWatched.load() << std::endl;
        
        ss << "# HELP sentinelfs_files_synced Total number of files synced" << std::endl;
        ss << "# TYPE sentinelfs_files_synced counter" << std::endl;
        ss << "sentinelfs_files_synced " << syncMetrics_.filesSynced.load() << std::endl;
        
        ss << "# HELP sentinelfs_sync_errors Total number of sync errors" << std::endl;
        ss << "# TYPE sentinelfs_sync_errors counter" << std::endl;
        ss << "sentinelfs_sync_errors " << syncMetrics_.syncErrors.load() << std::endl;
        
        // Network metrics
        ss << "# HELP sentinelfs_bytes_uploaded_total Total bytes uploaded" << std::endl;
        ss << "# TYPE sentinelfs_bytes_uploaded_total counter" << std::endl;
        ss << "sentinelfs_bytes_uploaded_total " << networkMetrics_.bytesUploaded.load() << std::endl;
        
        ss << "# HELP sentinelfs_bytes_downloaded_total Total bytes downloaded" << std::endl;
        ss << "# TYPE sentinelfs_bytes_downloaded_total counter" << std::endl;
        ss << "sentinelfs_bytes_downloaded_total " << networkMetrics_.bytesDownloaded.load() << std::endl;
        
        ss << "# HELP sentinelfs_peers_connected Current number of connected peers" << std::endl;
        ss << "# TYPE sentinelfs_peers_connected gauge" << std::endl;
        ss << "sentinelfs_peers_connected " << networkMetrics_.peersConnected.load() << std::endl;
        
        // Security metrics
        ss << "# HELP sentinelfs_anomalies_detected Total anomalies detected" << std::endl;
        ss << "# TYPE sentinelfs_anomalies_detected counter" << std::endl;
        ss << "sentinelfs_anomalies_detected " << securityMetrics_.anomaliesDetected.load() << std::endl;
        
        ss << "# HELP sentinelfs_auth_failures Total authentication failures" << std::endl;
        ss << "# TYPE sentinelfs_auth_failures counter" << std::endl;
        ss << "sentinelfs_auth_failures " << securityMetrics_.authFailures.load() << std::endl;
        
        // Performance metrics
        ss << "# HELP sentinelfs_sync_latency_ms Average sync latency in milliseconds" << std::endl;
        ss << "# TYPE sentinelfs_sync_latency_ms gauge" << std::endl;
        ss << "sentinelfs_sync_latency_ms " << perfMetrics_.avgSyncLatencyMs.load() << std::endl;
        
        ss << "# HELP sentinelfs_memory_usage_mb Peak memory usage in MB" << std::endl;
        ss << "# TYPE sentinelfs_memory_usage_mb gauge" << std::endl;
        ss << "sentinelfs_memory_usage_mb " << perfMetrics_.peakMemoryUsageMB.load() << std::endl;
        
        return ss.str();
    }

    void MetricsCollector::reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Reset sync metrics
        syncMetrics_.filesWatched = 0;
        syncMetrics_.filesSynced = 0;
        syncMetrics_.filesModified = 0;
        syncMetrics_.filesDeleted = 0;
        syncMetrics_.syncErrors = 0;
        syncMetrics_.conflictsDetected = 0;
        
        // Reset network metrics
        networkMetrics_.bytesUploaded = 0;
        networkMetrics_.bytesDownloaded = 0;
        networkMetrics_.peersDiscovered = 0;
        networkMetrics_.peersConnected = 0;
        networkMetrics_.peersDisconnected = 0;
        networkMetrics_.transfersCompleted = 0;
        networkMetrics_.transfersFailed = 0;
        networkMetrics_.deltasSent = 0;
        networkMetrics_.deltasReceived = 0;
        networkMetrics_.remeshCycles = 0;
        
        // Reset security metrics
        securityMetrics_.anomaliesDetected = 0;
        securityMetrics_.suspiciousActivities = 0;
        securityMetrics_.syncPausedCount = 0;
        securityMetrics_.authFailures = 0;
        securityMetrics_.encryptionErrors = 0;
        
        // Reset performance metrics
        perfMetrics_.avgSyncLatencyMs = 0;
        perfMetrics_.avgDeltaComputeTimeMs = 0;
        perfMetrics_.avgTransferSpeedKBps = 0;
        perfMetrics_.peakMemoryUsageMB = 0;
        perfMetrics_.cpuUsagePercent = 0;
        perfMetrics_.avgRemeshRttImprovementMs = 0;
        
        startTime_ = std::chrono::system_clock::now();
    }

    std::chrono::seconds MetricsCollector::getUptime() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);
    }

    void MetricsCollector::updateMovingAverage(std::atomic<uint64_t>& avg, uint64_t newValue) {
        // Simple exponential moving average with alpha = 0.2
        uint64_t current = avg.load();
        uint64_t updated = (current == 0) ? newValue : (current * 4 + newValue) / 5;
        avg = updated;
    }

} // namespace SentinelFS
