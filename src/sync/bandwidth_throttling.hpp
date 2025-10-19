#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <queue>
#include <condition_variable>
#include "../models.hpp"

// Transfer statistics
struct TransferStats {
    size_t bytesTransferred;
    size_t totalBytes;
    double transferRate;  // Bytes per second
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastUpdateTime;
    size_t currentSpeed;  // Instantaneous speed
    size_t averageSpeed;  // Average speed
    
    TransferStats() : bytesTransferred(0), totalBytes(0), transferRate(0.0),
                     currentSpeed(0), averageSpeed(0) {
        startTime = std::chrono::steady_clock::now();
        lastUpdateTime = startTime;
    }
};

// Bandwidth throttling configuration
struct BandwidthConfig {
    size_t maxUploadSpeed;     // Bytes per second (0 = unlimited)
    size_t maxDownloadSpeed;   // Bytes per second (0 = unlimited)
    size_t burstAllowance;     // Extra bandwidth allowance for bursts
    std::vector<int> allowedHours;  // Hours when full bandwidth allowed (0-23)
    bool enableThrottling;
    bool adaptiveThrottling;   // Automatically adjust based on network conditions
    
    BandwidthConfig() : maxUploadSpeed(0), maxDownloadSpeed(0), 
                       burstAllowance(1024 * 1024),  // 1MB burst by default
                       enableThrottling(false), adaptiveThrottling(false) {}
};

// Throttled transfer request
struct ThrottledTransfer {
    std::string filePath;
    size_t fileSize;
    size_t transferredBytes;
    bool isUpload;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastTransferTime;
    bool completed;
    bool paused;
    
    ThrottledTransfer() : fileSize(0), transferredBytes(0), isUpload(false),
                        completed(false), paused(false) {
        startTime = std::chrono::steady_clock::now();
        lastTransferTime = startTime;
    }
    
    ThrottledTransfer(const std::string& path, size_t size, bool upload)
        : filePath(path), fileSize(size), transferredBytes(0), isUpload(upload),
          completed(false), paused(false) {
        startTime = std::chrono::steady_clock::now();
        lastTransferTime = startTime;
    }
};

// Bandwidth limiter
class BandwidthLimiter {
public:
    BandwidthLimiter(const BandwidthConfig& config = BandwidthConfig());
    ~BandwidthLimiter();
    
    // Start/stop bandwidth monitoring
    void start();
    void stop();
    
    // Throttle data transfer
    bool throttleTransfer(const ThrottledTransfer& transfer, size_t bytesToTransfer);
    
    // Get current bandwidth usage
    double getCurrentUploadRate() const;
    double getCurrentDownloadRate() const;
    double getAverageUploadRate() const;
    double getAverageDownloadRate() const;
    
    // Get/set configuration
    BandwidthConfig getConfiguration() const;
    void setConfiguration(const BandwidthConfig& config);
    
    // Pause/resume transfers
    void pauseTransfer(const std::string& filePath);
    void resumeTransfer(const std::string& filePath);
    void cancelTransfer(const std::string& filePath);
    
    // Get transfer statistics
    std::vector<TransferStats> getTransferStatistics() const;
    TransferStats getTransferStats(const std::string& filePath) const;
    
    // Bandwidth monitoring
    bool isBandwidthAvailable(bool isUpload, size_t bytesNeeded) const;
    void waitForBandwidth(bool isUpload, size_t bytesNeeded);
    
    // Adaptive throttling
    void enableAdaptiveThrottling(bool enable);
    void updateNetworkConditions();
    
    // Time-based restrictions
    bool isTransferAllowed() const;
    std::vector<int> getAllowedHours() const;
    void setAllowedHours(const std::vector<int>& hours);
    
private:
    BandwidthConfig config;
    mutable std::mutex configMutex;
    
    std::atomic<double> currentUploadRate{0.0};
    std::atomic<double> currentDownloadRate{0.0};
    std::atomic<double> averageUploadRate{0.0};
    std::atomic<double> averageDownloadRate{0.0};
    
    std::atomic<bool> running{false};
    std::thread monitoringThread;
    
    mutable std::mutex transfersMutex;
    std::vector<ThrottledTransfer> activeTransfers;
    std::map<std::string, TransferStats> transferStats;
    
    std::chrono::steady_clock::time_point lastUpdate;
    std::vector<double> recentUploadRates;
    std::vector<double> recentDownloadRates;
    
    // Synchronization primitives
    std::condition_variable bandwidthCondition;
    mutable std::mutex bandwidthMutex;
    
    // Internal methods
    void monitoringLoop();
    void updateTransferRates();
    void calculateAverages();
    bool isWithinAllowedHours() const;
    size_t calculateBurstAllowance() const;
    
    // Rate limiting
    void limitTransferRate(bool isUpload, size_t bytesTransferred);
    void sleepIfNeeded(bool isUpload, size_t bytesTransferred);
    
    // Adaptive throttling methods
    void adjustThrottlingBasedOnNetwork();
    double calculateNetworkUtilization() const;
    void adjustTransferPriorities();
};