#include "FileSyncHandler.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <filesystem>

namespace SentinelFS {

FileSyncHandler::FileSyncHandler(INetworkAPI* network, IStorageAPI* storage, const std::string& watchDir)
    : network_(network), storage_(storage), watchDirectory_(watchDir) {
    auto& logger = Logger::instance();
    logger.debug("FileSyncHandler initialized for: " + watchDir, "FileSyncHandler");
}

void FileSyncHandler::handleFileModified(const std::string& fullPath) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    std::string filename = std::filesystem::path(fullPath).filename().string();
    
    // Check if sync is enabled first
    if (!syncEnabled_) {
        logger.debug("Sync disabled, skipping update for: " + filename, "FileSyncHandler");
        return;
    }
    
    try {
        // Get connected peers
        auto peers = storage_->getAllPeers();
        
        if (peers.empty()) {
            logger.debug("No peers connected, skipping broadcast for: " + filename, "FileSyncHandler");
            return;
        }
        
        logger.info("Broadcasting update for " + filename + " to " + std::to_string(peers.size()) + " peer(s)", "FileSyncHandler");
        
        // Broadcast UPDATE_AVAILABLE to all peers with full path
        std::string updateMsg = "UPDATE_AVAILABLE|" + fullPath;
        std::vector<uint8_t> payload(updateMsg.begin(), updateMsg.end());
        
        int successCount = 0;
        int failCount = 0;
        
        for (const auto& peer : peers) {
            try {
                bool sent = network_->sendData(peer.id, payload);
                if (sent) {
                    successCount++;
                    logger.debug("Sent update notification to peer: " + peer.id, "FileSyncHandler");
                } else {
                    failCount++;
                    logger.warn("Failed to send update to peer: " + peer.id, "FileSyncHandler");
                }
            } catch (const std::exception& e) {
                failCount++;
                logger.error("Exception sending to peer " + peer.id + ": " + std::string(e.what()), "FileSyncHandler");
            }
        }
        
        if (successCount > 0) {
            metrics.incrementFilesSynced();
        }
        
        if (failCount > 0) {
            metrics.incrementSyncErrors();
            logger.warn("Update broadcast completed with " + std::to_string(failCount) + " failure(s)", "FileSyncHandler");
        } else {
            logger.debug("Update broadcast successful to all peers", "FileSyncHandler");
        }
        
    } catch (const std::exception& e) {
        logger.error("Error in handleFileModified: " + std::string(e.what()), "FileSyncHandler");
        metrics.incrementSyncErrors();
    }
}

} // namespace SentinelFS
