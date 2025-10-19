#include "bandwidth_throttling.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <thread>
#include <chrono>
#include <numeric>

BandwidthLimiter::BandwidthLimiter(const BandwidthConfig& config)
    : config(config), lastUpdate(std::chrono::steady_clock::now()) {
}

BandwidthLimiter::~BandwidthLimiter() {
    stop();
}

void BandwidthLimiter::start() {
    if (running.exchange(true)) {
        return;  // Already running
    }
    
    monitoringThread = std::thread(&BandwidthLimiter::monitoringLoop, this);
}

void BandwidthLimiter::stop() {
    if (running.exchange(false)) {
        if (monitoringThread.joinable()) {
            monitoringThread.join();
        }
    }
}

bool BandwidthLimiter::throttleTransfer(const ThrottledTransfer& transfer, size_t bytesToTransfer) {
    if (!config.enableThrottling) {
        return true;  // No throttling needed
    }
    
    std::lock_guard<std::mutex> lock(transfersMutex);
    
    // Check if transfer is paused or completed
    if (transfer.paused || transfer.completed) {
        return false;
    }
    
    // Check bandwidth limits
    bool isUpload = transfer.isUpload;
    size_t maxSpeed = isUpload ? config.maxUploadSpeed : config.maxDownloadSpeed;
    
    // If unlimited bandwidth, allow transfer
    if (maxSpeed == 0) {
        return true;
    }
    
    // Check if burst allowance can cover this transfer
    size_t burstAllowance = calculateBurstAllowance();
    if (bytesToTransfer <= burstAllowance) {
        return true;  // Within burst allowance
    }
    
    // Limit based on configured speed
    limitTransferRate(isUpload, bytesToTransfer);
    
    return true;
}

double BandwidthLimiter::getCurrentUploadRate() const {
    return currentUploadRate.load();
}

double BandwidthLimiter::getCurrentDownloadRate() const {
    return currentDownloadRate.load();
}

double BandwidthLimiter::getAverageUploadRate() const {
    return averageUploadRate.load();
}

double BandwidthLimiter::getAverageDownloadRate() const {
    return averageDownloadRate.load();
}

BandwidthConfig BandwidthLimiter::getConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config;
}

void BandwidthLimiter::setConfiguration(const BandwidthConfig& newConfig) {
    std::lock_guard<std::mutex> lock(configMutex);
    config = newConfig;
}

void BandwidthLimiter::pauseTransfer(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(transfersMutex);
    
    auto it = std::find_if(activeTransfers.begin(), activeTransfers.end(),
                          [&filePath](const ThrottledTransfer& t) { 
                              return t.filePath == filePath; 
                          });
    
    if (it != activeTransfers.end()) {
        it->paused = true;
    }
}

void BandwidthLimiter::resumeTransfer(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(transfersMutex);
    
    auto it = std::find_if(activeTransfers.begin(), activeTransfers.end(),
                          [&filePath](const ThrottledTransfer& t) { 
                              return t.filePath == filePath; 
                          });
    
    if (it != activeTransfers.end()) {
        it->paused = false;
    }
}

void BandwidthLimiter::cancelTransfer(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(transfersMutex);
    
    activeTransfers.erase(
        std::remove_if(activeTransfers.begin(), activeTransfers.end(),
                      [&filePath](const ThrottledTransfer& t) { 
                          return t.filePath == filePath; 
                      }),
        activeTransfers.end());
    
    // Remove from stats
    transferStats.erase(filePath);
}

std::vector<TransferStats> BandwidthLimiter::getTransferStatistics() const {
    std::lock_guard<std::mutex> lock(transfersMutex);
    
    std::vector<TransferStats> stats;
    for (const auto& pair : transferStats) {
        stats.push_back(pair.second);
    }
    
    return stats;
}

TransferStats BandwidthLimiter::getTransferStats(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(transfersMutex);
    
    auto it = transferStats.find(filePath);
    if (it != transferStats.end()) {
        return it->second;
    }
    
    return TransferStats();
}

bool BandwidthLimiter::isBandwidthAvailable(bool isUpload, size_t bytesNeeded) const {
    if (!config.enableThrottling) {
        return true;  // No throttling enabled
    }
    
    size_t maxSpeed = isUpload ? config.maxUploadSpeed : config.maxDownloadSpeed;
    
    if (maxSpeed == 0) {
        return true;  // Unlimited bandwidth
    }
    
    double currentRate = isUpload ? getCurrentUploadRate() : getCurrentDownloadRate();
    
    // Check if adding bytesNeeded would exceed limit
    return (currentRate + bytesNeeded) <= maxSpeed;
}

void BandwidthLimiter::waitForBandwidth(bool isUpload, size_t bytesNeeded) {
    while (!isBandwidthAvailable(isUpload, bytesNeeded)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void BandwidthLimiter::enableAdaptiveThrottling(bool enable) {
    std::lock_guard<std::mutex> lock(configMutex);
    config.adaptiveThrottling = enable;
}

void BandwidthLimiter::updateNetworkConditions() {
    if (config.adaptiveThrottling) {
        adjustThrottlingBasedOnNetwork();
    }
}

bool BandwidthLimiter::isTransferAllowed() const {
    if (config.allowedHours.empty()) {
        return true;  // No time restrictions
    }
    
    return isWithinAllowedHours();
}

std::vector<int> BandwidthLimiter::getAllowedHours() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.allowedHours;
}

void BandwidthLimiter::setAllowedHours(const std::vector<int>& hours) {
    std::lock_guard<std::mutex> lock(configMutex);
    config.allowedHours = hours;
}

void BandwidthLimiter::monitoringLoop() {
    const auto updateInterval = std::chrono::seconds(1);
    
    while (running.load()) {
        updateTransferRates();
        calculateAverages();
        updateNetworkConditions();
        
        std::this_thread::sleep_for(updateInterval);
    }
}

void BandwidthLimiter::updateTransferRates() {
    auto now = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() / 1000.0;
    
    if (elapsedTime <= 0.0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(transfersMutex);
    
    // Calculate current transfer rates
    size_t totalUploadBytes = 0;
    size_t totalDownloadBytes = 0;
    
    for (const auto& transfer : activeTransfers) {
        auto it = transferStats.find(transfer.filePath);
        if (it != transferStats.end()) {
            size_t bytesSinceLastUpdate = it->second.bytesTransferred - 
                                          it->second.bytesTransferred;  // This should be calculated properly
            if (transfer.isUpload) {
                totalUploadBytes += bytesSinceLastUpdate;
            } else {
                totalDownloadBytes += bytesSinceLastUpdate;
            }
        }
    }
    
    // Update current rates
    currentUploadRate = totalUploadBytes / elapsedTime;
    currentDownloadRate = totalDownloadBytes / elapsedTime;
    
    lastUpdate = now;
}

void BandwidthLimiter::calculateAverages() {
    double currentUpload = currentUploadRate.load();
    double currentDownload = currentDownloadRate.load();
    
    // Maintain rolling window of recent rates (last 10 seconds)
    recentUploadRates.push_back(currentUpload);
    recentDownloadRates.push_back(currentDownload);
    
    if (recentUploadRates.size() > 10) {
        recentUploadRates.erase(recentUploadRates.begin());
    }
    
    if (recentDownloadRates.size() > 10) {
        recentDownloadRates.erase(recentDownloadRates.begin());
    }
    
    // Calculate average rates
    if (!recentUploadRates.empty()) {
        double avgUpload = std::accumulate(recentUploadRates.begin(), 
                                         recentUploadRates.end(), 0.0) / recentUploadRates.size();
        averageUploadRate = avgUpload;
    }
    
    if (!recentDownloadRates.empty()) {
        double avgDownload = std::accumulate(recentDownloadRates.begin(), 
                                           recentDownloadRates.end(), 0.0) / recentDownloadRates.size();
        averageDownloadRate = avgDownload;
    }
}

bool BandwidthLimiter::isWithinAllowedHours() const {
    std::lock_guard<std::mutex> lock(configMutex);
    
    if (config.allowedHours.empty()) {
        return true;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    tm* timeinfo = localtime(&time_t);
    
    int currentHour = timeinfo->tm_hour;
    
    return std::find(config.allowedHours.begin(), config.allowedHours.end(), currentHour) 
           != config.allowedHours.end();
}

size_t BandwidthLimiter::calculateBurstAllowance() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.burstAllowance;
}

void BandwidthLimiter::limitTransferRate(bool isUpload, size_t bytesTransferred) {
    size_t maxSpeed = isUpload ? config.maxUploadSpeed : config.maxDownloadSpeed;
    
    if (maxSpeed == 0) {
        return;  // Unlimited speed
    }
    
    double currentRate = isUpload ? getCurrentUploadRate() : getCurrentDownloadRate();
    
    if (currentRate > maxSpeed) {
        // Calculate sleep time needed to stay within limit
        double excessRate = currentRate - maxSpeed;
        double sleepTime = (excessRate / maxSpeed) * 1000;  // Convert to milliseconds
        
        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long>(sleepTime)));
        }
    }
}

void BandwidthLimiter::sleepIfNeeded(bool isUpload, size_t bytesTransferred) {
    size_t maxSpeed = isUpload ? config.maxUploadSpeed : config.maxDownloadSpeed;
    
    if (maxSpeed == 0) {
        return;  // Unlimited speed
    }
    
    double currentRate = isUpload ? getCurrentUploadRate() : getCurrentDownloadRate();
    
    if (currentRate > maxSpeed * 0.9) {  // 90% of limit
        // Slow down to stay within bounds
        double slowDownFactor = maxSpeed / std::max(1.0, currentRate);
        long sleepMs = static_cast<long>((1.0 - slowDownFactor) * 100);
        
        if (sleepMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
    }
}

void BandwidthLimiter::adjustThrottlingBasedOnNetwork() {
    // Calculate current network utilization
    double utilization = calculateNetworkUtilization();
    
    std::lock_guard<std::mutex> lock(configMutex);
    
    if (utilization > 0.8) {  // High utilization
        // Reduce bandwidth limits by 20%
        if (config.maxUploadSpeed > 0) {
            config.maxUploadSpeed = static_cast<size_t>(config.maxUploadSpeed * 0.8);
        }
        if (config.maxDownloadSpeed > 0) {
            config.maxDownloadSpeed = static_cast<size_t>(config.maxDownloadSpeed * 0.8);
        }
    } else if (utilization < 0.3) {  // Low utilization
        // Increase bandwidth limits by 10% (but not above original limits)
        // Note: In a real implementation, you'd track original limits
        if (config.maxUploadSpeed > 0) {
            config.maxUploadSpeed = static_cast<size_t>(config.maxUploadSpeed * 1.1);
        }
        if (config.maxDownloadSpeed > 0) {
            config.maxDownloadSpeed = static_cast<size_t>(config.maxDownloadSpeed * 1.1);
        }
    }
}

double BandwidthLimiter::calculateNetworkUtilization() const {
    double uploadUtilization = 0.0;
    double downloadUtilization = 0.0;
    
    std::lock_guard<std::mutex> lock(configMutex);
    
    if (config.maxUploadSpeed > 0) {
        uploadUtilization = getCurrentUploadRate() / config.maxUploadSpeed;
    }
    
    if (config.maxDownloadSpeed > 0) {
        downloadUtilization = getCurrentDownloadRate() / config.maxDownloadSpeed;
    }
    
    // Return average utilization
    return (uploadUtilization + downloadUtilization) / 2.0;
}

void BandwidthLimiter::adjustTransferPriorities() {
    std::lock_guard<std::mutex> lock(transfersMutex);
    
    // Sort transfers by priority and remaining bytes
    std::sort(activeTransfers.begin(), activeTransfers.end(),
             [](const ThrottledTransfer& a, const ThrottledTransfer& b) {
                 // Prioritize by remaining bytes (larger first) and completion status
                 size_t remainingA = a.fileSize - a.transferredBytes;
                 size_t remainingB = b.fileSize - b.transferredBytes;
                 
                 if (remainingA != remainingB) {
                     return remainingA > remainingB;  // Larger transfers first
                 }
                 
                 // If same size, prioritize completed transfers
                 if (a.completed != b.completed) {
                     return !a.completed && b.completed;  // Non-completed first
                 }
                 
                 return false;  // Equal priority
             });
}

// Utility functions for bandwidth management
namespace BandwidthUtils {
    std::string formatSpeed(size_t bytesPerSecond) {
        if (bytesPerSecond < 1024) {
            return std::to_string(bytesPerSecond) + " B/s";
        } else if (bytesPerSecond < 1024 * 1024) {
            return std::to_string(bytesPerSecond / 1024) + " KB/s";
        } else {
            return std::to_string(bytesPerSecond / (1024 * 1024)) + " MB/s";
        }
    }
    
    std::string formatSize(size_t bytes) {
        if (bytes < 1024) {
            return std::to_string(bytes) + " B";
        } else if (bytes < 1024 * 1024) {
            return std::to_string(bytes / 1024) + " KB";
        } else if (bytes < 1024 * 1024 * 1024) {
            return std::to_string(bytes / (1024 * 1024)) + " MB";
        } else {
            return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
        }
    }
}