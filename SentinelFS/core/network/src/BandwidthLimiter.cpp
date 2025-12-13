#include "BandwidthLimiter.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <algorithm>
#include <iostream>

namespace SentinelFS {

BandwidthLimiter::BandwidthLimiter(size_t maxBytesPerSecond, size_t burstCapacity, double targetDelay)
    : maxBytesPerSecond_(maxBytesPerSecond)
    , burstCapacity_(burstCapacity > 0 ? burstCapacity : maxBytesPerSecond * 2)
    , tokens_(burstCapacity_)
    , lastRefill_(std::chrono::steady_clock::now())
    , targetDelay_(targetDelay)
    , currentRate_(maxBytesPerSecond)
    , lastRateAdjustment_(std::chrono::steady_clock::now())
{
    if (maxBytesPerSecond_ == 0) {
        // Unlimited
        burstCapacity_ = 0;
        tokens_ = 0;
        currentRate_ = 0;
    }
}

void BandwidthLimiter::refillTokens() {
    if (!isEnabled()) return;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastRefill_).count();
    
    if (elapsed > 0) {
        // Add tokens based on elapsed time
        double tokensToAdd = (maxBytesPerSecond_ * elapsed) / 1000000.0;
        tokens_ = std::min(tokens_ + tokensToAdd, static_cast<double>(burstCapacity_));
        lastRefill_ = now;
    }
}

bool BandwidthLimiter::requestTransfer(size_t bytes) {
    if (!isEnabled()) {
        totalBytesTransferred_ += bytes;
        return true;
    }
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    auto startWait = std::chrono::steady_clock::now();
    
    while (tokens_ < bytes) {
        refillTokens();
        
        if (tokens_ >= bytes) break;
        
        // Calculate how long to wait
        double tokensNeeded = bytes - tokens_;
        auto waitMicros = static_cast<long long>((tokensNeeded / maxBytesPerSecond_) * 1000000);
        
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(std::min(waitMicros, 100000LL)));
        lock.lock();
    }
    
    tokens_ -= bytes;
    totalBytesTransferred_ += bytes;
    
    auto endWait = std::chrono::steady_clock::now();
    auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(endWait - startWait).count();
    totalWaitTimeMs_ += waitTime;
    
    return true;
}

size_t BandwidthLimiter::tryTransfer(size_t bytes) {
    if (!isEnabled()) {
        totalBytesTransferred_ += bytes;
        return bytes;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    refillTokens();
    
    size_t allowed = std::min(bytes, static_cast<size_t>(tokens_));
    if (allowed > 0) {
        tokens_ -= allowed;
        totalBytesTransferred_ += allowed;
    }
    
    return allowed;
}

void BandwidthLimiter::setRateLimit(size_t maxBytesPerSecond) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    maxBytesPerSecond_ = maxBytesPerSecond;
    
    if (maxBytesPerSecond_ == 0) {
        // Unlimited
        burstCapacity_ = 0;
        tokens_ = 0;
    } else {
        burstCapacity_ = maxBytesPerSecond * 2;
        tokens_ = std::min(tokens_, static_cast<double>(burstCapacity_));
    }
    
    lastRefill_ = std::chrono::steady_clock::now();
}

void BandwidthLimiter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    tokens_ = burstCapacity_;
    lastRefill_ = std::chrono::steady_clock::now();
}

std::pair<uint64_t, uint64_t> BandwidthLimiter::getStats() const {
    return {totalBytesTransferred_.load(), totalWaitTimeMs_.load()};
}

// BandwidthManager implementation

BandwidthManager::BandwidthManager(size_t globalUploadLimit, size_t globalDownloadLimit)
    : globalUpload_(globalUploadLimit)
    , globalDownload_(globalDownloadLimit)
{
}

bool BandwidthManager::requestUpload(const std::string& peerId, size_t bytes) {
    // Check per-peer limit first
    {
        std::lock_guard<std::mutex> lock(peerMutex_);
        auto it = peerUploadLimiters_.find(peerId);
        if (it != peerUploadLimiters_.end()) {
            if (!it->second->requestTransfer(bytes)) {
                return false;
            }
        }
    }
    
    // Then check global limit
    return globalUpload_.requestTransfer(bytes);
}

bool BandwidthManager::requestDownload(const std::string& peerId, size_t bytes) {
    // Check per-peer limit first
    {
        std::lock_guard<std::mutex> lock(peerMutex_);
        auto it = peerDownloadLimiters_.find(peerId);
        if (it != peerDownloadLimiters_.end()) {
            if (!it->second->requestTransfer(bytes)) {
                return false;
            }
        }
    }
    
    // Then check global limit
    return globalDownload_.requestTransfer(bytes);
}

void BandwidthManager::setGlobalUploadLimit(size_t bytesPerSecond) {
    auto& logger = Logger::instance();
    
    globalUpload_.setRateLimit(bytesPerSecond);
    if (bytesPerSecond > 0) {
        logger.log(LogLevel::INFO, "Global upload limit set to " + std::to_string(bytesPerSecond / 1024) + " KB/s", "BandwidthManager");
    } else {
        logger.log(LogLevel::INFO, "Global upload limit set to unlimited", "BandwidthManager");
    }
}

void BandwidthManager::setGlobalDownloadLimit(size_t bytesPerSecond) {
    auto& logger = Logger::instance();
    
    globalDownload_.setRateLimit(bytesPerSecond);
    if (bytesPerSecond > 0) {
        logger.log(LogLevel::INFO, "Global download limit set to " + std::to_string(bytesPerSecond / 1024) + " KB/s", "BandwidthManager");
    } else {
        logger.log(LogLevel::INFO, "Global download limit set to unlimited", "BandwidthManager");
    }
}

void BandwidthManager::setPeerUploadLimit(const std::string& peerId, size_t bytesPerSecond) {
    auto& logger = Logger::instance();
    
    std::lock_guard<std::mutex> lock(peerMutex_);
    peerUploadLimiters_[peerId] = std::make_unique<BandwidthLimiter>(bytesPerSecond);
    logger.log(LogLevel::INFO, "Peer " + peerId + " upload limit: " + std::to_string(bytesPerSecond / 1024) + " KB/s", "BandwidthManager");
}

void BandwidthManager::setPeerDownloadLimit(const std::string& peerId, size_t bytesPerSecond) {
    auto& logger = Logger::instance();
    
    std::lock_guard<std::mutex> lock(peerMutex_);
    peerDownloadLimiters_[peerId] = std::make_unique<BandwidthLimiter>(bytesPerSecond);
    logger.log(LogLevel::INFO, "Peer " + peerId + " download limit: " + std::to_string(bytesPerSecond / 1024) + " KB/s", "BandwidthManager");
}

void BandwidthManager::removePeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(peerMutex_);
    peerUploadLimiters_.erase(peerId);
    peerDownloadLimiters_.erase(peerId);
}

BandwidthManager::BandwidthStats BandwidthManager::getStats() const {
    BandwidthStats stats;
    
    stats.globalUploadLimit = globalUpload_.getRateLimit();
    stats.globalDownloadLimit = globalDownload_.getRateLimit();
    
    auto [uploadBytes, uploadWait] = globalUpload_.getStats();
    auto [downloadBytes, downloadWait] = globalDownload_.getStats();
    
    stats.totalUploaded = uploadBytes;
    stats.totalDownloaded = downloadBytes;
    stats.uploadWaitMs = uploadWait;
    stats.downloadWaitMs = downloadWait;
    
    // Calculate current rates (simplified - would need time windowing for accuracy)
    stats.currentUploadRate = stats.globalUploadLimit;
    stats.currentDownloadRate = stats.globalDownloadLimit;
    
    {
        std::lock_guard<std::mutex> lock(peerMutex_);
        stats.activePeers = peerUploadLimiters_.size();
    }
    
    return stats;
}

void BandwidthLimiter::updateRTT(double rttMs) {
    if (!congestionControlEnabled_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Update RTT history
    rttHistory_.push_back(rttMs);
    if (rttHistory_.size() > RTT_HISTORY_SIZE) {
        rttHistory_.pop_front();
    }
    
    // Update current RTT (use the latest measurement)
    currentRTT_ = rttMs;
    
    // Update minimum RTT (use the minimum over the window)
    updateMinRTT(rttMs);
    
    // Calculate queue delay
    queueDelay_ = currentRTT_ - minRTT_;
    if (queueDelay_ < 0) queueDelay_ = 0;
    
    // Adjust rate based on congestion
    adjustRate();
}

void BandwidthLimiter::updateMinRTT(double rtt) {
    // Initialize minRTT_ on first measurement
    if (minRTT_ == 1000.0) {
        minRTT_ = rtt;
        return;
    }
    
    // Find minimum RTT in the history window
    double newMinRTT = rtt;
    for (double histRtt : rttHistory_) {
        if (histRtt < newMinRTT) {
            newMinRTT = histRtt;
        }
    }
    
    // Update minRTT_ with some hysteresis to avoid fluctuations
    if (newMinRTT < minRTT_) {
        minRTT_ = newMinRTT;
    } else if (newMinRTT > minRTT_ * 1.1) {
        // Allow minRTT to increase slowly (10% threshold)
        minRTT_ = minRTT_ * 1.01;
    }
}

void BandwidthLimiter::adjustRate() {
    if (!isEnabled() || currentRate_ == 0) return;
    
    auto now = std::chrono::steady_clock::now();
    auto timeSinceAdjustment = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastRateAdjustment_).count();
    
    // Only adjust rate every 100ms to avoid oscillations
    if (timeSinceAdjustment < 100) return;
    
    lastRateAdjustment_ = now;
    
    // LEDBAT rate adjustment formula
    // rate += GAIN * (targetDelay - queueDelay) * rate / targetDelay
    double offTarget = targetDelay_ - queueDelay_;
    double rateAdjustment = GAIN * offTarget * currentRate_ / targetDelay_;
    
    size_t newRate = static_cast<size_t>(currentRate_ + rateAdjustment);
    
    // Apply bounds
    if (newRate < MIN_RATE) {
        newRate = MIN_RATE;
    } else if (newRate > maxBytesPerSecond_) {
        newRate = maxBytesPerSecond_;
    }
    
    // Limit rate increases to avoid aggressive behavior
    if (newRate > currentRate_) {
        size_t maxIncrease = static_cast<size_t>(currentRate_ * (MAX_RATE_INCREASE - 1.0));
        if (newRate - currentRate_ > maxIncrease) {
            newRate = currentRate_ + maxIncrease;
        }
    }
    
    if (newRate != currentRate_) {
        currentRate_ = newRate;
        
        // Update token bucket parameters based on new rate
        maxBytesPerSecond_ = currentRate_;
        burstCapacity_ = currentRate_ * 2;
        
        Logger::instance().log(LogLevel::DEBUG, 
            "Adjusted rate to " + std::to_string(currentRate_) + " bytes/s " +
            "(RTT: " + std::to_string(currentRTT_) + "ms, " +
            "queue delay: " + std::to_string(queueDelay_) + "ms)",
            "BandwidthLimiter");
    }
}

void BandwidthLimiter::reportPacketLoss(size_t packetsLost, size_t totalPackets) {
    if (!congestionControlEnabled_ || totalPackets == 0) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    packetsLost_ += packetsLost;
    packetsSent_ += totalPackets;
    
    // On packet loss, reduce rate significantly
    if (packetsLost > 0) {
        size_t newRate = static_cast<size_t>(currentRate_ * DECREASE_FACTOR);
        if (newRate < MIN_RATE) newRate = MIN_RATE;
        
        currentRate_ = newRate;
        maxBytesPerSecond_ = currentRate_;
        burstCapacity_ = currentRate_ * 2;
        
        Logger::instance().log(LogLevel::WARN,
            "Packet loss detected, reducing rate to " + std::to_string(currentRate_) + " bytes/s",
            "BandwidthLimiter");
    }
}

CongestionStats BandwidthLimiter::getCongestionStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    CongestionStats stats;
    stats.currentRTT = currentRTT_;
    stats.minRTT = minRTT_;
    stats.queueDelay = queueDelay_;
    stats.targetDelay = targetDelay_;
    stats.currentRate = currentRate_;
    stats.packetsLost = packetsLost_;
    stats.lossRate = packetsSent_ > 0 ? static_cast<double>(packetsLost_) / packetsSent_ : 0.0;
    
    return stats;
}

} // namespace SentinelFS
