#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <set>
#include <functional>
#include <thread>
#include "../models.hpp"

// Transfer checkpoint for resumable transfers
struct TransferCheckpoint {
    std::string filePath;
    std::string transferId;
    size_t totalSize;
    size_t transferredBytes;
    size_t chunkSize;
    std::vector<size_t> completedChunks;
    std::chrono::steady_clock::time_point lastCheckpoint;
    std::string checksum;  // For integrity verification
    int retryAttempts;
    std::string peerId;
    bool isUpload;
    
    TransferCheckpoint() : totalSize(0), transferredBytes(0), chunkSize(1024*1024), 
                          retryAttempts(0), isUpload(false) {
        lastCheckpoint = std::chrono::steady_clock::now();
    }
    
    TransferCheckpoint(const std::string& path, size_t size, const std::string& id = "")
        : filePath(path), transferId(id), totalSize(size), transferredBytes(0), 
          chunkSize(1024*1024), retryAttempts(0), isUpload(false) {
        if (transferId.empty()) {
            transferId = generateTransferId();
        }
        lastCheckpoint = std::chrono::steady_clock::now();
    }
    
private:
    std::string generateTransferId() const;
};

// Network disconnection event
struct DisconnectionEvent {
    std::string peerId;
    std::chrono::steady_clock::time_point disconnectTime;
    std::chrono::steady_clock::time_point reconnectTime;
    std::string reason;
    bool recovered;
    
    DisconnectionEvent() : disconnectTime(std::chrono::steady_clock::now()), 
                          recovered(false) {}
};

// Resumable transfer manager
class ResumableTransferManager {
public:
    ResumableTransferManager();
    ~ResumableTransferManager();
    
    // Start/stop manager
    void start();
    void stop();
    
    // Create/checkpoint management
    TransferCheckpoint createCheckpoint(const std::string& filePath, size_t fileSize,
                                      const std::string& peerId, bool isUpload = true);
    bool saveCheckpoint(const TransferCheckpoint& checkpoint);
    TransferCheckpoint loadCheckpoint(const std::string& transferId) const;
    bool removeCheckpoint(const std::string& transferId);
    bool hasCheckpoint(const std::string& transferId) const;
    
    // Resume transfer
    bool resumeTransfer(const std::string& transferId);
    bool resumeTransferFromFile(const std::string& filePath, const std::string& peerId);
    
    // Transfer recovery
    bool recoverInterruptedTransfers();
    std::vector<TransferCheckpoint> getPendingTransfers() const;
    std::vector<TransferCheckpoint> getFailedTransfers() const;
    
    // Network event handling
    void handleDisconnection(const std::string& peerId, const std::string& reason = "");
    void handleReconnection(const std::string& peerId);
    std::vector<DisconnectionEvent> getConnectionHistory(const std::string& peerId) const;
    
    // Retry management
    void setMaxRetryAttempts(int maxRetries);
    int getMaxRetryAttempts() const;
    void resetRetryCount(const std::string& transferId);
    
    // Chunk management
    std::vector<size_t> getMissingChunks(const TransferCheckpoint& checkpoint) const;
    bool markChunkComplete(const std::string& transferId, size_t chunkIndex);
    size_t getNextChunkToTransfer(const TransferCheckpoint& checkpoint) const;
    
    // Integrity verification
    bool verifyTransferIntegrity(const TransferCheckpoint& checkpoint) const;
    std::string calculateFileChecksum(const std::string& filePath) const;
    
    // Progress tracking
    double getTransferProgress(const std::string& transferId) const;
    std::chrono::seconds getEstimatedTimeRemaining(const std::string& transferId) const;
    
    // Cleanup
    void cleanupOldCheckpoints(std::chrono::hours maxAge = std::chrono::hours(24*7)); // 1 week
    void cleanupCompletedTransfers();
    
    // Event callbacks
    using TransferCompleteCallback = std::function<void(const std::string& transferId, bool success)>;
    using TransferProgressCallback = std::function<void(const std::string& transferId, double progress)>;
    using TransferErrorCallback = std::function<void(const std::string& transferId, const std::string& error)>;
    
    void setTransferCompleteCallback(TransferCompleteCallback callback);
    void setTransferProgressCallback(TransferProgressCallback callback);
    void setTransferErrorCallback(TransferErrorCallback callback);
    
    // Statistics
    size_t getTotalPendingTransfers() const;
    size_t getTotalRecoveredTransfers() const;
    double getRecoverySuccessRate() const;
    
private:
    mutable std::mutex checkpointsMutex;
    std::map<std::string, TransferCheckpoint> activeCheckpoints;
    std::map<std::string, TransferCheckpoint> completedCheckpoints;
    std::map<std::string, int> transferRetryCounts;
    
    mutable std::mutex networkMutex;
    std::map<std::string, std::vector<DisconnectionEvent>> connectionHistory;
    
    std::atomic<bool> running{false};
    std::thread recoveryThread;
    
    std::atomic<int> maxRetryAttempts{3};
    std::atomic<size_t> recoveredTransfers{0};
    std::atomic<size_t> failedTransfers{0};
    
    // Callbacks
    TransferCompleteCallback transferCompleteCallback;
    TransferProgressCallback transferProgressCallback;
    TransferErrorCallback transferErrorCallback;
    
    // Configuration
    std::string checkpointDirectory;
    bool autoCleanup;
    std::chrono::hours maxCheckpointAge;
    
    // Internal methods
    void recoveryLoop();
    bool attemptTransferRecovery(TransferCheckpoint& checkpoint);
    void notifyTransferComplete(const std::string& transferId, bool success);
    void notifyTransferProgress(const std::string& transferId, double progress);
    void notifyTransferError(const std::string& transferId, const std::string& error);
    
    // File operations
    bool saveCheckpointToFile(const TransferCheckpoint& checkpoint) const;
    TransferCheckpoint loadCheckpointFromFile(const std::string& transferId) const;
    bool removeCheckpointFile(const std::string& transferId) const;
    
    // Utilities
    std::string generateUniqueTransferId() const;
    bool isTransferStale(const TransferCheckpoint& checkpoint) const;
    void updateTransferStats(const std::string& transferId, bool success);
};