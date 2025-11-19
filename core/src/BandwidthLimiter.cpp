#include "BandwidthLimiter.h"
#include <algorithm>
#include <iostream>

namespace SentinelFS {

BandwidthLimiter::BandwidthLimiter(size_t maxBytesPerSecond, size_t burstCapacity)
    : maxBytesPerSecond_(maxBytesPerSecond)
    , burstCapacity_(burstCapacity > 0 ? burstCapacity : maxBytesPerSecond * 2)
    , tokens_(burstCapacity_)
    , lastRefill_(std::chrono::steady_clock::now())
{
    if (maxBytesPerSecond_ == 0) {
        // Unlimited
        burstCapacity_ = 0;
        tokens_ = 0;
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
    globalUpload_.setRateLimit(bytesPerSecond);
    std::cout << "Global upload limit set to " << bytesPerSecond << " B/s";
    if (bytesPerSecond > 0) {
        std::cout << " (" << (bytesPerSecond / 1024) << " KB/s)" << std::endl;
    } else {
        std::cout << " (unlimited)" << std::endl;
    }
}

void BandwidthManager::setGlobalDownloadLimit(size_t bytesPerSecond) {
    globalDownload_.setRateLimit(bytesPerSecond);
    std::cout << "Global download limit set to " << bytesPerSecond << " B/s";
    if (bytesPerSecond > 0) {
        std::cout << " (" << (bytesPerSecond / 1024) << " KB/s)" << std::endl;
    } else {
        std::cout << " (unlimited)" << std::endl;
    }
}

void BandwidthManager::setPeerUploadLimit(const std::string& peerId, size_t bytesPerSecond) {
    std::lock_guard<std::mutex> lock(peerMutex_);
    peerUploadLimiters_[peerId] = std::make_unique<BandwidthLimiter>(bytesPerSecond);
    std::cout << "Peer " << peerId << " upload limit: " << (bytesPerSecond / 1024) << " KB/s" << std::endl;
}

void BandwidthManager::setPeerDownloadLimit(const std::string& peerId, size_t bytesPerSecond) {
    std::lock_guard<std::mutex> lock(peerMutex_);
    peerDownloadLimiters_[peerId] = std::make_unique<BandwidthLimiter>(bytesPerSecond);
    std::cout << "Peer " << peerId << " download limit: " << (bytesPerSecond / 1024) << " KB/s" << std::endl;
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
    
    // Calculate current rates (simplified - would need time windowing for accuracy)
    stats.currentUploadRate = stats.globalUploadLimit;
    stats.currentDownloadRate = stats.globalDownloadLimit;
    
    {
        std::lock_guard<std::mutex> lock(peerMutex_);
        stats.activePeers = peerUploadLimiters_.size();
    }
    
    return stats;
}

} // namespace SentinelFS
