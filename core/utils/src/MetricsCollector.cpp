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
    
    // ML Threat Detection metrics
    void MetricsCollector::incrementThreatsDetected() { securityMetrics_.threatsDetected++; }
    void MetricsCollector::incrementRansomwareAlerts() { securityMetrics_.ransomwareAlerts++; }
    void MetricsCollector::incrementHighEntropyFiles() { securityMetrics_.highEntropyFiles++; }
    void MetricsCollector::incrementMassOperationAlerts() { securityMetrics_.massOperationAlerts++; }
    void MetricsCollector::updateThreatScore(double score) { 
        // score is 0-1, convert to 0-100 percentage for storage and display
        securityMetrics_.currentThreatScoreX100 = static_cast<uint64_t>(score * 100 * 100); 
    }
    void MetricsCollector::updateAvgFileEntropy(double entropy) { 
        securityMetrics_.avgFileEntropyX100 = static_cast<uint64_t>(entropy * 100); 
    }
    void MetricsCollector::resetThreatMetrics() {
        securityMetrics_.threatsDetected = 0;
        securityMetrics_.ransomwareAlerts = 0;
        securityMetrics_.highEntropyFiles = 0;
        securityMetrics_.massOperationAlerts = 0;
        securityMetrics_.currentThreatScoreX100 = 0;
        securityMetrics_.avgFileEntropyX100 = 0;
    }

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
        // ML Threat Detection metrics
        snapshot.threatsDetected = securityMetrics_.threatsDetected.load();
        snapshot.ransomwareAlerts = securityMetrics_.ransomwareAlerts.load();
        snapshot.highEntropyFiles = securityMetrics_.highEntropyFiles.load();
        snapshot.massOperationAlerts = securityMetrics_.massOperationAlerts.load();
        snapshot.currentThreatScore = securityMetrics_.currentThreatScoreX100.load() / 100.0;
        snapshot.avgFileEntropy = securityMetrics_.avgFileEntropyX100.load() / 100.0;
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
        auto uptime = getUptime();
        
        // === Daemon Info ===
        ss << "# HELP sentinelfs_info SentinelFS daemon information" << std::endl;
        ss << "# TYPE sentinelfs_info gauge" << std::endl;
        ss << "sentinelfs_info{version=\"1.0.0\"} 1" << std::endl;
        
        ss << "# HELP sentinelfs_uptime_seconds Daemon uptime in seconds" << std::endl;
        ss << "# TYPE sentinelfs_uptime_seconds counter" << std::endl;
        ss << "sentinelfs_uptime_seconds " << uptime.count() << std::endl;
        
        // === Sync Metrics ===
        ss << "# HELP sentinelfs_files_watched_total Total number of files being watched" << std::endl;
        ss << "# TYPE sentinelfs_files_watched_total gauge" << std::endl;
        ss << "sentinelfs_files_watched_total " << syncMetrics_.filesWatched.load() << std::endl;
        
        ss << "# HELP sentinelfs_files_synced_total Total number of files synced" << std::endl;
        ss << "# TYPE sentinelfs_files_synced_total counter" << std::endl;
        ss << "sentinelfs_files_synced_total " << syncMetrics_.filesSynced.load() << std::endl;
        
        ss << "# HELP sentinelfs_files_modified_total Total number of file modifications detected" << std::endl;
        ss << "# TYPE sentinelfs_files_modified_total counter" << std::endl;
        ss << "sentinelfs_files_modified_total " << syncMetrics_.filesModified.load() << std::endl;
        
        ss << "# HELP sentinelfs_files_deleted_total Total number of file deletions detected" << std::endl;
        ss << "# TYPE sentinelfs_files_deleted_total counter" << std::endl;
        ss << "sentinelfs_files_deleted_total " << syncMetrics_.filesDeleted.load() << std::endl;
        
        ss << "# HELP sentinelfs_sync_errors_total Total number of sync errors" << std::endl;
        ss << "# TYPE sentinelfs_sync_errors_total counter" << std::endl;
        ss << "sentinelfs_sync_errors_total " << syncMetrics_.syncErrors.load() << std::endl;
        
        ss << "# HELP sentinelfs_conflicts_detected_total Total number of conflicts detected" << std::endl;
        ss << "# TYPE sentinelfs_conflicts_detected_total counter" << std::endl;
        ss << "sentinelfs_conflicts_detected_total " << syncMetrics_.conflictsDetected.load() << std::endl;
        
        // === Network Metrics ===
        ss << "# HELP sentinelfs_bytes_uploaded_total Total bytes uploaded to peers" << std::endl;
        ss << "# TYPE sentinelfs_bytes_uploaded_total counter" << std::endl;
        ss << "sentinelfs_bytes_uploaded_total " << networkMetrics_.bytesUploaded.load() << std::endl;
        
        ss << "# HELP sentinelfs_bytes_downloaded_total Total bytes downloaded from peers" << std::endl;
        ss << "# TYPE sentinelfs_bytes_downloaded_total counter" << std::endl;
        ss << "sentinelfs_bytes_downloaded_total " << networkMetrics_.bytesDownloaded.load() << std::endl;
        
        ss << "# HELP sentinelfs_peers_discovered_total Total number of peers discovered" << std::endl;
        ss << "# TYPE sentinelfs_peers_discovered_total counter" << std::endl;
        ss << "sentinelfs_peers_discovered_total " << networkMetrics_.peersDiscovered.load() << std::endl;
        
        ss << "# HELP sentinelfs_peers_connected Current number of connected peers" << std::endl;
        ss << "# TYPE sentinelfs_peers_connected gauge" << std::endl;
        ss << "sentinelfs_peers_connected " << networkMetrics_.peersConnected.load() << std::endl;
        
        ss << "# HELP sentinelfs_peers_disconnected_total Total number of peer disconnections" << std::endl;
        ss << "# TYPE sentinelfs_peers_disconnected_total counter" << std::endl;
        ss << "sentinelfs_peers_disconnected_total " << networkMetrics_.peersDisconnected.load() << std::endl;
        
        ss << "# HELP sentinelfs_transfers_completed_total Total number of successful file transfers" << std::endl;
        ss << "# TYPE sentinelfs_transfers_completed_total counter" << std::endl;
        ss << "sentinelfs_transfers_completed_total " << networkMetrics_.transfersCompleted.load() << std::endl;
        
        ss << "# HELP sentinelfs_transfers_failed_total Total number of failed file transfers" << std::endl;
        ss << "# TYPE sentinelfs_transfers_failed_total counter" << std::endl;
        ss << "sentinelfs_transfers_failed_total " << networkMetrics_.transfersFailed.load() << std::endl;
        
        ss << "# HELP sentinelfs_deltas_sent_total Total number of delta sync operations sent" << std::endl;
        ss << "# TYPE sentinelfs_deltas_sent_total counter" << std::endl;
        ss << "sentinelfs_deltas_sent_total " << networkMetrics_.deltasSent.load() << std::endl;
        
        ss << "# HELP sentinelfs_deltas_received_total Total number of delta sync operations received" << std::endl;
        ss << "# TYPE sentinelfs_deltas_received_total counter" << std::endl;
        ss << "sentinelfs_deltas_received_total " << networkMetrics_.deltasReceived.load() << std::endl;
        
        ss << "# HELP sentinelfs_remesh_cycles_total Total number of auto-remesh cycles executed" << std::endl;
        ss << "# TYPE sentinelfs_remesh_cycles_total counter" << std::endl;
        ss << "sentinelfs_remesh_cycles_total " << networkMetrics_.remeshCycles.load() << std::endl;
        
        // Active transfers
        {
            std::lock_guard<std::mutex> lock(transferMutex_);
            ss << "# HELP sentinelfs_active_transfers Current number of active file transfers" << std::endl;
            ss << "# TYPE sentinelfs_active_transfers gauge" << std::endl;
            ss << "sentinelfs_active_transfers " << activeTransfers_.size() << std::endl;
        }
        
        // === Security Metrics ===
        ss << "# HELP sentinelfs_anomalies_detected_total Total anomalies detected by ML plugin" << std::endl;
        ss << "# TYPE sentinelfs_anomalies_detected_total counter" << std::endl;
        ss << "sentinelfs_anomalies_detected_total " << securityMetrics_.anomaliesDetected.load() << std::endl;
        
        ss << "# HELP sentinelfs_suspicious_activities_total Total suspicious activities detected" << std::endl;
        ss << "# TYPE sentinelfs_suspicious_activities_total counter" << std::endl;
        ss << "sentinelfs_suspicious_activities_total " << securityMetrics_.suspiciousActivities.load() << std::endl;
        
        ss << "# HELP sentinelfs_sync_paused_total Total times sync was paused due to security" << std::endl;
        ss << "# TYPE sentinelfs_sync_paused_total counter" << std::endl;
        ss << "sentinelfs_sync_paused_total " << securityMetrics_.syncPausedCount.load() << std::endl;
        
        ss << "# HELP sentinelfs_auth_failures_total Total authentication failures" << std::endl;
        ss << "# TYPE sentinelfs_auth_failures_total counter" << std::endl;
        ss << "sentinelfs_auth_failures_total " << securityMetrics_.authFailures.load() << std::endl;
        
        ss << "# HELP sentinelfs_encryption_errors_total Total encryption/decryption errors" << std::endl;
        ss << "# TYPE sentinelfs_encryption_errors_total counter" << std::endl;
        ss << "sentinelfs_encryption_errors_total " << securityMetrics_.encryptionErrors.load() << std::endl;
        
        // === ML Threat Detection Metrics ===
        ss << "# HELP sentinelfs_threats_detected_total Total threats detected by ML engine" << std::endl;
        ss << "# TYPE sentinelfs_threats_detected_total counter" << std::endl;
        ss << "sentinelfs_threats_detected_total " << securityMetrics_.threatsDetected.load() << std::endl;
        
        ss << "# HELP sentinelfs_ransomware_alerts_total Total ransomware alerts generated" << std::endl;
        ss << "# TYPE sentinelfs_ransomware_alerts_total counter" << std::endl;
        ss << "sentinelfs_ransomware_alerts_total " << securityMetrics_.ransomwareAlerts.load() << std::endl;
        
        ss << "# HELP sentinelfs_high_entropy_files_total Total high-entropy files detected" << std::endl;
        ss << "# TYPE sentinelfs_high_entropy_files_total counter" << std::endl;
        ss << "sentinelfs_high_entropy_files_total " << securityMetrics_.highEntropyFiles.load() << std::endl;
        
        ss << "# HELP sentinelfs_mass_operation_alerts_total Total mass operation alerts" << std::endl;
        ss << "# TYPE sentinelfs_mass_operation_alerts_total counter" << std::endl;
        ss << "sentinelfs_mass_operation_alerts_total " << securityMetrics_.massOperationAlerts.load() << std::endl;
        
        ss << "# HELP sentinelfs_current_threat_score Current unified threat score (0-1)" << std::endl;
        ss << "# TYPE sentinelfs_current_threat_score gauge" << std::endl;
        ss << std::fixed << std::setprecision(3);
        ss << "sentinelfs_current_threat_score " << (securityMetrics_.currentThreatScoreX100.load() / 100.0) << std::endl;
        
        ss << "# HELP sentinelfs_avg_file_entropy Average file entropy (0-8 bits)" << std::endl;
        ss << "# TYPE sentinelfs_avg_file_entropy gauge" << std::endl;
        ss << "sentinelfs_avg_file_entropy " << (securityMetrics_.avgFileEntropyX100.load() / 100.0) << std::endl;
        
        // === Performance Metrics ===
        ss << "# HELP sentinelfs_sync_latency_ms Average sync latency in milliseconds" << std::endl;
        ss << "# TYPE sentinelfs_sync_latency_ms gauge" << std::endl;
        ss << "sentinelfs_sync_latency_ms " << perfMetrics_.avgSyncLatencyMs.load() << std::endl;
        
        ss << "# HELP sentinelfs_delta_compute_time_ms Average delta computation time in milliseconds" << std::endl;
        ss << "# TYPE sentinelfs_delta_compute_time_ms gauge" << std::endl;
        ss << "sentinelfs_delta_compute_time_ms " << perfMetrics_.avgDeltaComputeTimeMs.load() << std::endl;
        
        ss << "# HELP sentinelfs_transfer_speed_kbps Average transfer speed in KB/s" << std::endl;
        ss << "# TYPE sentinelfs_transfer_speed_kbps gauge" << std::endl;
        ss << "sentinelfs_transfer_speed_kbps " << perfMetrics_.avgTransferSpeedKBps.load() << std::endl;
        
        ss << "# HELP sentinelfs_memory_usage_mb Peak memory usage in megabytes" << std::endl;
        ss << "# TYPE sentinelfs_memory_usage_mb gauge" << std::endl;
        ss << "sentinelfs_memory_usage_mb " << perfMetrics_.peakMemoryUsageMB.load() << std::endl;
        
        ss << "# HELP sentinelfs_cpu_usage_percent Current CPU usage percentage" << std::endl;
        ss << "# TYPE sentinelfs_cpu_usage_percent gauge" << std::endl;
        ss << "sentinelfs_cpu_usage_percent " << perfMetrics_.cpuUsagePercent.load() << std::endl;
        
        ss << "# HELP sentinelfs_remesh_rtt_improvement_ms Average RTT improvement from auto-remesh in ms" << std::endl;
        ss << "# TYPE sentinelfs_remesh_rtt_improvement_ms gauge" << std::endl;
        ss << "sentinelfs_remesh_rtt_improvement_ms " << perfMetrics_.avgRemeshRttImprovementMs.load() << std::endl;
        
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

    std::string MetricsCollector::startTransfer(const std::string& filePath, const std::string& peerId, bool isUpload, uint64_t totalBytes) {
        std::lock_guard<std::mutex> lock(transferMutex_);
        
        std::string transferId = "transfer_" + std::to_string(++transferIdCounter_);
        
        ActiveTransferInfo info;
        info.transferId = transferId;
        info.filePath = filePath;
        info.peerId = peerId;
        info.isUpload = isUpload;
        info.totalBytes = totalBytes;
        info.transferredBytes = 0;
        info.speedBps = 0;
        info.progress = 0;
        info.startTime = std::chrono::steady_clock::now();
        
        activeTransfers_[transferId] = info;
        
        return transferId;
    }

    void MetricsCollector::updateTransferProgress(const std::string& transferId, uint64_t transferredBytes) {
        std::lock_guard<std::mutex> lock(transferMutex_);
        
        auto it = activeTransfers_.find(transferId);
        if (it == activeTransfers_.end()) return;
        
        auto& info = it->second;
        info.transferredBytes = transferredBytes;
        
        // Calculate progress
        if (info.totalBytes > 0) {
            info.progress = static_cast<int>((transferredBytes * 100) / info.totalBytes);
        }
        
        // Calculate speed
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - info.startTime).count();
        if (elapsed > 0) {
            info.speedBps = (transferredBytes * 1000) / static_cast<uint64_t>(elapsed);
        }
    }

    void MetricsCollector::completeTransfer(const std::string& transferId, bool success) {
        std::lock_guard<std::mutex> lock(transferMutex_);
        
        auto it = activeTransfers_.find(transferId);
        if (it != activeTransfers_.end()) {
            activeTransfers_.erase(it);
        }
        
        if (success) {
            incrementTransfersCompleted();
        } else {
            incrementTransfersFailed();
        }
    }

    std::vector<ActiveTransferInfo> MetricsCollector::getActiveTransfers() const {
        std::lock_guard<std::mutex> lock(transferMutex_);
        
        std::vector<ActiveTransferInfo> result;
        result.reserve(activeTransfers_.size());
        
        for (const auto& pair : activeTransfers_) {
            result.push_back(pair.second);
        }
        
        return result;
    }

} // namespace SentinelFS
