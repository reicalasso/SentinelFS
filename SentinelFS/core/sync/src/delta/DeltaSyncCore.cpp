/**
 * @file DeltaSyncCore.cpp
 * @brief DeltaSyncProtocolHandler constructor, destructor and cleanup thread management
 */

#include "DeltaSyncProtocolHandler.h"
#include "Logger.h"
#include <chrono>

namespace SentinelFS {

DeltaSyncProtocolHandler::DeltaSyncProtocolHandler(INetworkAPI* network, IStorageAPI* storage,
                                                   IFileAPI* filesystem, const std::string& watchDir)
    : network_(network), storage_(storage), filesystem_(filesystem), watchDirectory_(watchDir) {
    auto& logger = Logger::instance();
    logger.debug("DeltaSyncProtocolHandler initialized for: " + watchDir, "DeltaSyncProtocol");
    startCleanupThread();
}

DeltaSyncProtocolHandler::~DeltaSyncProtocolHandler() {
    stopCleanupThread();
}

void DeltaSyncProtocolHandler::startCleanupThread() {
    cleanupRunning_ = true;
    cleanupThread_ = std::thread([this]() {
        auto& logger = Logger::instance();
        logger.debug("Pending chunks cleanup thread started", "DeltaSyncProtocol");
        
        while (cleanupRunning_) {
            // Sleep in small intervals to allow quick shutdown
            for (int i = 0; i < CLEANUP_INTERVAL_SECONDS && cleanupRunning_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            if (cleanupRunning_) {
                cleanupStaleChunks();
            }
        }
        
        logger.debug("Pending chunks cleanup thread stopped", "DeltaSyncProtocol");
    });
}

void DeltaSyncProtocolHandler::stopCleanupThread() {
    cleanupRunning_ = false;
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
}

void DeltaSyncProtocolHandler::cleanupStaleChunks() {
    auto& logger = Logger::instance();
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(pendingMutex_);
    
    size_t cleanedCount = 0;
    for (auto it = pendingDeltas_.begin(); it != pendingDeltas_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastActivity).count();
        
        if (elapsed > CHUNK_TIMEOUT_SECONDS) {
            logger.warn("Cleaning up stale pending chunks for: " + it->first + 
                       " (idle for " + std::to_string(elapsed) + "s)", "DeltaSyncProtocol");
            it = pendingDeltas_.erase(it);
            ++cleanedCount;
        } else {
            ++it;
        }
    }
    
    if (cleanedCount > 0) {
        logger.info("Cleaned up " + std::to_string(cleanedCount) + " stale pending chunk entries", "DeltaSyncProtocol");
    }
}

} // namespace SentinelFS
