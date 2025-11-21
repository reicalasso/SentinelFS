#pragma once

#include <chrono>
#include <mutex>
#include <atomic>
#include <thread>
#include <map>
#include <memory>
#include <string>

namespace SentinelFS {

/**
 * @brief Token Bucket algorithm for bandwidth throttling
 * 
 * Implements smooth rate limiting with burst capacity.
 * Tokens are added at a constant rate (bytes per second).
 * Each transfer consumes tokens. If no tokens available, transfer waits.
 */
class BandwidthLimiter {
public:
    /**
     * @brief Construct a bandwidth limiter
     * @param maxBytesPerSecond Maximum transfer rate (0 = unlimited)
     * @param burstCapacity Maximum burst size in bytes (default: 2x rate)
     */
    BandwidthLimiter(size_t maxBytesPerSecond = 0, size_t burstCapacity = 0);
    
    /**
     * @brief Request permission to transfer bytes
     * @param bytes Number of bytes to transfer
     * @return true if transfer allowed immediately, false if rate limited
     * 
     * This method will block until sufficient tokens are available.
     */
    bool requestTransfer(size_t bytes);
    
    /**
     * @brief Try to transfer without blocking
     * @param bytes Number of bytes to transfer
     * @return Number of bytes allowed (may be less than requested)
     */
    size_t tryTransfer(size_t bytes);
    
    /**
     * @brief Update the rate limit
     * @param maxBytesPerSecond New rate limit (0 = unlimited)
     */
    void setRateLimit(size_t maxBytesPerSecond);
    
    /**
     * @brief Get current rate limit
     * @return Bytes per second (0 = unlimited)
     */
    size_t getRateLimit() const { return maxBytesPerSecond_; }
    
    /**
     * @brief Check if rate limiting is enabled
     */
    bool isEnabled() const { return maxBytesPerSecond_ > 0; }
    
    /**
     * @brief Get current token count
     */
    double getCurrentTokens() const { return tokens_; }
    
    /**
     * @brief Reset token bucket to full capacity
     */
    void reset();
    
    /**
     * @brief Get statistics
     * @return Pair of (total_bytes_transferred, total_wait_time_ms)
     */
    std::pair<uint64_t, uint64_t> getStats() const;

private:
    void refillTokens();
    
    size_t maxBytesPerSecond_;  // Rate limit (0 = unlimited)
    size_t burstCapacity_;      // Maximum tokens in bucket
    double tokens_;             // Current token count
    
    std::chrono::steady_clock::time_point lastRefill_;
    mutable std::mutex mutex_;
    
    // Statistics
    std::atomic<uint64_t> totalBytesTransferred_{0};
    std::atomic<uint64_t> totalWaitTimeMs_{0};
};

/**
 * @brief Bandwidth manager for multiple peers
 * 
 * Distributes available bandwidth fairly across active peers.
 */
class BandwidthManager {
public:
    BandwidthManager(size_t globalUploadLimit = 0, size_t globalDownloadLimit = 0);
    
    /**
     * @brief Request upload bandwidth for a peer
     * @param peerId Peer ID
     * @param bytes Bytes to upload
     * @return true if allowed
     */
    bool requestUpload(const std::string& peerId, size_t bytes);
    
    /**
     * @brief Request download bandwidth for a peer
     * @param peerId Peer ID
     * @param bytes Bytes to download
     * @return true if allowed
     */
    bool requestDownload(const std::string& peerId, size_t bytes);
    
    /**
     * @brief Set global upload limit
     */
    void setGlobalUploadLimit(size_t bytesPerSecond);
    
    /**
     * @brief Set global download limit
     */
    void setGlobalDownloadLimit(size_t bytesPerSecond);
    
    /**
     * @brief Set per-peer limit (overrides fair share)
     */
    void setPeerUploadLimit(const std::string& peerId, size_t bytesPerSecond);
    void setPeerDownloadLimit(const std::string& peerId, size_t bytesPerSecond);
    
    /**
     * @brief Remove peer limits
     */
    void removePeer(const std::string& peerId);
    
    /**
     * @brief Get bandwidth statistics
     */
    struct BandwidthStats {
        size_t globalUploadLimit;
        size_t globalDownloadLimit;
        size_t currentUploadRate;
        size_t currentDownloadRate;
        uint64_t totalUploaded;
        uint64_t totalDownloaded;
        uint64_t uploadWaitMs;
        uint64_t downloadWaitMs;
        size_t activePeers;
    };
    
    BandwidthStats getStats() const;

private:
    BandwidthLimiter globalUpload_;
    BandwidthLimiter globalDownload_;
    
    std::map<std::string, std::unique_ptr<BandwidthLimiter>> peerUploadLimiters_;
    std::map<std::string, std::unique_ptr<BandwidthLimiter>> peerDownloadLimiters_;
    
    mutable std::mutex peerMutex_;
};

/**
 * @brief Priority queue for file transfers
 */
enum class TransferPriority {
    CRITICAL = 0,   // Small files, config files (< 1MB)
    HIGH = 1,       // Medium files (1-10MB)
    NORMAL = 2,     // Large files (10-100MB)
    LOW = 3,        // Very large files (> 100MB)
    BACKGROUND = 4  // Bulk transfers, backups
};

struct TransferTask {
    std::string filePath;
    std::string peerId;
    size_t fileSize;
    TransferPriority priority;
    std::chrono::steady_clock::time_point queuedAt;
    
    // Compare for priority queue (higher priority = lower enum value)
    bool operator<(const TransferTask& other) const {
        if (priority != other.priority) {
            return priority > other.priority;  // Reversed for max-heap
        }
        return queuedAt > other.queuedAt;  // FIFO within same priority
    }
};

} // namespace SentinelFS
