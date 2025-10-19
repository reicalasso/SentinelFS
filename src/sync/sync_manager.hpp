#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <set>
#include "../models.hpp"
#include "selective_sync.hpp"
#include "bandwidth_throttling.hpp"
#include "resume_transfers.hpp"
#include "version_history.hpp"

// Sync configuration
struct SyncConfig {
    bool enableSelectiveSync;          // Enable selective sync
    bool enableBandwidthThrottling;    // Enable bandwidth throttling
    bool enableResumeTransfers;         // Enable resumable transfers
    bool enableVersionHistory;         // Enable version history
    size_t maxBandwidthUpload;         // Max upload bandwidth in bytes/sec (0 = unlimited)
    size_t maxBandwidthDownload;       // Max download bandwidth in bytes/sec (0 = unlimited)
    std::vector<std::string> syncPatterns;  // Patterns to include/exclude
    std::vector<int> allowedSyncHours;  // Hours when sync is allowed (0-23)
    bool adaptiveBandwidth;            // Auto-adjust bandwidth based on network conditions
    bool enableCompression;            // Compress data during transfer
    size_t maxVersionsPerFile;         // Max versions to keep per file (0 = unlimited)
    std::chrono::hours maxVersionAge;  // Max age of versions (0 = unlimited)
    
    SyncConfig() : enableSelectiveSync(true), enableBandwidthThrottling(true),
                  enableResumeTransfers(true), enableVersionHistory(true),
                  maxBandwidthUpload(0), maxBandwidthDownload(0),
                  adaptiveBandwidth(true), enableCompression(true),
                  maxVersionsPerFile(10), maxVersionAge(std::chrono::hours(24*30)) {}  // 30 days
};

// Sync event
struct SyncEvent {
    enum class Type {
        FILE_ADDED,
        FILE_MODIFIED,
        FILE_DELETED,
        FILE_CONFLICT,
        TRANSFER_STARTED,
        TRANSFER_COMPLETED,
        TRANSFER_FAILED,
        TRANSFER_RESUMED,
        VERSION_CREATED,
        VERSION_RESTORED,
        BANDWIDTH_LIMITED,
        NETWORK_ERROR,
        SECURITY_ALERT
    };
    
    Type type;
    std::string filePath;
    std::string peerId;
    size_t fileSize;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    bool success;
    
    SyncEvent(Type t, const std::string& path = "", const std::string& peer = "")
        : type(t), filePath(path), peerId(peer), fileSize(0), success(true) {
        timestamp = std::chrono::system_clock::now();
    }
};

// Sync statistics
struct SyncStats {
    size_t filesSynced;
    size_t bytesTransferred;
    size_t transferFailures;
    size_t conflictsResolved;
    size_t versionsCreated;
    double averageBandwidth;
    std::chrono::system_clock::time_point lastSync;
    
    SyncStats() : filesSynced(0), bytesTransferred(0), transferFailures(0),
                 conflictsResolved(0), versionsCreated(0), averageBandwidth(0.0) {
        lastSync = std::chrono::system_clock::now();
    }
};

// Main sync manager that integrates all features
class SyncManager {
public:
    SyncManager(const SyncConfig& config = SyncConfig());
    ~SyncManager();
    
    // Start/stop sync manager
    bool start();
    void stop();
    
    // Configuration management
    void setConfig(const SyncConfig& config);
    SyncConfig getConfig() const;
    
    // File sync operations
    bool syncFile(const std::string& filePath, const std::string& peerId = "");
    bool syncDirectory(const std::string& directoryPath);
    void cancelSync(const std::string& filePath);
    
    // Selective sync management
    void addSyncRule(const SyncRule& rule);
    void removeSyncRule(const std::string& pattern);
    std::vector<SyncRule> getSyncRules() const;
    bool shouldSyncFile(const std::string& filePath, size_t fileSize = 0) const;
    SyncPriority getFilePriority(const std::string& filePath) const;
    
    // Bandwidth management
    void setBandwidthLimits(size_t maxUpload, size_t maxDownload);
    void setBandwidthThrottling(bool enable);
    double getCurrentUploadRate() const;
    double getCurrentDownloadRate() const;
    void pauseAllTransfers();
    void resumeAllTransfers();
    
    // Resume transfers
    bool resumeInterruptedTransfer(const std::string& transferId);
    std::vector<TransferCheckpoint> getPendingTransfers() const;
    std::vector<TransferCheckpoint> getFailedTransfers() const;
    double getTransferProgress(const std::string& transferId) const;
    
    // Version history
    FileVersion createFileVersion(const std::string& filePath, 
                                 const std::string& commitMessage = "",
                                 const std::string& modifiedBy = "");
    bool restoreFileVersion(const std::string& versionId, 
                            const std::string& restorePath = "");
    std::vector<FileVersion> getFileVersions(const std::string& filePath) const;
    bool deleteOldVersions(const std::string& filePath);
    
    // Network event handling
    void handleNetworkDisconnect(const std::string& peerId, const std::string& reason = "");
    void handleNetworkReconnect(const std::string& peerId);
    bool isNetworkStable() const;
    
    // Conflict resolution
    void addConflictResolutionRule(const std::string& pattern, ConflictResolutionStrategy strategy);
    ConflictResolutionStrategy getConflictResolutionStrategy(const std::string& filePath) const;
    bool resolveConflict(const std::string& filePath, const std::vector<PeerInfo>& peers);
    
    // Performance monitoring
    SyncStats getSyncStats() const;
    void resetSyncStats();
    std::vector<SyncEvent> getRecentEvents(size_t limit = 100) const;
    double getSyncEfficiency() const;
    
    // Resource management
    void cleanupOldVersions();
    void optimizeStorage();
    size_t getStorageUsage() const;
    void setStorageQuota(size_t quotaBytes);
    
    // Event callbacks
    using SyncEventCallback = std::function<void(const SyncEvent&)>;
    void setSyncEventCallback(SyncEventCallback callback);
    
    // Security features
    void enableFileEncryption(bool enable);
    bool isFileEncrypted(const std::string& filePath) const;
    void addTrustedPeer(const std::string& peerId);
    bool isPeerTrusted(const std::string& peerId) const;
    
    // Maintenance
    void runMaintenance();
    bool verifyIntegrity() const;
    void rebuildIndices();
    
private:
    SyncConfig config;
    mutable std::mutex configMutex;
    
    // Component managers
    std::unique_ptr<SelectiveSyncManager> selectiveSync;
    std::unique_ptr<BandwidthLimiter> bandwidthLimiter;
    std::unique_ptr<ResumableTransferManager> resumableTransfers;
    std::unique_ptr<VersionHistoryManager> versionHistory;
    
    // State management
    std::atomic<bool> running{false};
    std::atomic<bool> paused{false};
    std::mutex stateMutex;
    
    // Event handling
    std::vector<SyncEvent> eventLog;
    mutable std::mutex eventMutex;
    SyncEventCallback syncEventCallback;
    
    // Statistics
    mutable std::mutex statsMutex;
    SyncStats syncStats;
    
    // Configuration
    std::map<std::string, ConflictResolutionStrategy> conflictResolutionRules;
    std::set<std::string> trustedPeers;
    size_t storageQuota;
    
    // Internal methods
    void syncLoop();
    bool processFileSync(const std::string& filePath, const std::string& peerId);
    void logEvent(const SyncEvent& event);
    void updateStats(const SyncEvent& event);
    bool checkBandwidthAvailability(const std::string& filePath, size_t fileSize, bool isUpload);
    void handleTransferFailure(const std::string& filePath, const std::string& error);
    void handleTransferSuccess(const std::string& filePath, size_t bytesTransferred);
    
    // Component initialization
    void initializeComponents();
    void configureBandwidthLimiter();
    void configureVersionHistory();
    
    // Utilities
    std::string getFileType(const std::string& filePath) const;
    bool matchesPattern(const std::string& filePath, const std::string& pattern) const;
    bool isWithinSyncHours() const;
    void waitForBandwidthAvailability(const std::string& filePath, size_t fileSize, bool isUpload);
    
    // Thread management
    std::thread syncThread;
    std::thread maintenanceThread;
    std::atomic<bool> maintenanceRunning{false};
    
    void maintenanceLoop();
    void adaptiveBandwidthAdjustment();
    void storageCleanup();
};