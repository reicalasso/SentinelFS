/**
 * @file UpdateHandler.cpp
 * @brief Handle UPDATE_AVAILABLE messages - request delta from peer
 */

#include "DeltaSyncProtocolHandler.h"
#include "DeltaSerialization.h"
#include "DeltaEngine.h"
#include "INetworkAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>

namespace SentinelFS {

void DeltaSyncProtocolHandler::handleUpdateAvailable(const std::string& peerId, 
                                                     const std::vector<uint8_t>& rawData) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    try {
        std::string fullMsg(rawData.begin(), rawData.end());
        
        // Parse message: UPDATE_AVAILABLE|relativePath|hash|size
        const std::string prefix = "UPDATE_AVAILABLE|";
        if (fullMsg.size() <= prefix.length()) {
            logger.error("Invalid UPDATE_AVAILABLE message format", "DeltaSyncProtocol");
            return;
        }
        
        std::string payload = fullMsg.substr(prefix.length());
        
        // Parse fields
        size_t firstPipe = payload.find('|');
        size_t secondPipe = payload.find('|', firstPipe + 1);
        
        std::string relativePath = payload;
        std::string remoteHash;
        
        if (firstPipe != std::string::npos) {
            relativePath = payload.substr(0, firstPipe);
            if (secondPipe != std::string::npos) {
                remoteHash = payload.substr(firstPipe + 1, secondPipe - firstPipe - 1);
                // remoteSize parsed but not currently used for delta sync decision
                try {
                    (void)std::stoll(payload.substr(secondPipe + 1));
                } catch (...) {}
            }
        }
        
        // Convert path to local absolute path
        std::string localPath;
        std::string filename = std::filesystem::path(relativePath).filename().string();
        
        if (!relativePath.empty() && relativePath[0] == '/') {
            // Absolute path from peer - use just the filename under our watch directory
            localPath = watchDirectory_;
            if (!localPath.empty() && localPath.back() != '/') {
                localPath += '/';
            }
            localPath += filename;
        } else {
            // Relative path - append to watch directory
            localPath = watchDirectory_;
            if (!localPath.empty() && localPath.back() != '/') {
                localPath += '/';
            }
            localPath += relativePath;
        }
        
        logger.info("Peer " + peerId + " has update for: " + filename + " (remote hash: " + remoteHash.substr(0, 8) + "...)", "DeltaSyncProtocol");
        
        // Check if we already have this version (same hash)
        if (std::filesystem::exists(localPath) && !remoteHash.empty()) {
            // Quick check - if file exists and we want to avoid unnecessary sync
            // We could compare hashes here, but for now just proceed with delta
        }
        
        // Calculate local signature
        std::vector<BlockSignature> sigs;
        if (std::filesystem::exists(localPath)) {
            logger.debug("Calculating signature for existing file: " + filename, "DeltaSyncProtocol");
            sigs = DeltaEngine::calculateSignature(localPath);
        } else {
            logger.debug("File doesn't exist locally, requesting full copy: " + filename, "DeltaSyncProtocol");
        }
        
        auto serializedSig = DeltaSerialization::serializeSignature(sigs);
        
        // Send delta request with relative path (peer will resolve to their local path)
        std::string header = "REQUEST_DELTA|" + relativePath + "|";
        std::vector<uint8_t> payloadData(header.begin(), header.end());
        payloadData.insert(payloadData.end(), serializedSig.begin(), serializedSig.end());
        
        bool sent = network_->sendData(peerId, payloadData);
        if (sent) {
            logger.debug("Sent delta request to peer " + peerId, "DeltaSyncProtocol");
        } else {
            logger.warn("Failed to send delta request to peer " + peerId, "DeltaSyncProtocol");
            metrics.incrementTransfersFailed();
        }
        
    } catch (const std::exception& e) {
        logger.error("Error in handleUpdateAvailable: " + std::string(e.what()), "DeltaSyncProtocol");
        metrics.incrementSyncErrors();
    }
}

} // namespace SentinelFS
