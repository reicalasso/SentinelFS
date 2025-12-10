/**
 * @file DeltaDataHandler.cpp
 * @brief Handle DELTA_DATA messages - receive and apply delta
 */

#include "DeltaSyncProtocolHandler.h"
#include "DeltaSerialization.h"
#include "DeltaEngine.h"
#include "IFileAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace SentinelFS {

void DeltaSyncProtocolHandler::handleDeltaData(const std::string& peerId,
                                               const std::vector<uint8_t>& rawData) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        std::string msg(rawData.begin(), rawData.end());
        
        size_t firstPipe = msg.find('|');
        size_t secondPipe = msg.find('|', firstPipe + 1);
        
        if (secondPipe == std::string::npos) {
            logger.error("Invalid DELTA_DATA message format", "DeltaSyncProtocol");
            return;
        }
        
        // This is the path from peer (might be relative or absolute)
        std::string remotePath = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        std::string filename = std::filesystem::path(remotePath).filename().string();
        
        // Convert to local absolute path
        std::string localPath;
        if (!remotePath.empty() && remotePath[0] == '/') {
            // Absolute path - use just the filename under our watch directory
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
            localPath += remotePath;
        }
        
        // For chunk assembly, use remotePath as key component
        std::string relativePath = remotePath;
        
        // Check if this is a chunked DELTA_DATA message: DELTA_DATA|relativePath|chunkId/total|...
        size_t thirdPipe = msg.find('|', secondPipe + 1);
        if (thirdPipe != std::string::npos) {
            std::string chunkInfo = msg.substr(secondPipe + 1, thirdPipe - secondPipe - 1);
            size_t slashPos = chunkInfo.find('/');
            if (slashPos != std::string::npos) {
                std::string chunkIdStr = chunkInfo.substr(0, slashPos);
                std::string totalStr = chunkInfo.substr(slashPos + 1);
                std::uint32_t chunkId = static_cast<std::uint32_t>(std::stoul(chunkIdStr));
                std::uint32_t totalChunks = static_cast<std::uint32_t>(std::stoul(totalStr));

                if (totalChunks == 0) {
                    logger.error("Invalid delta chunk header (totalChunks=0) for " + filename, "DeltaSyncProtocol");
                    return;
                }

                std::string key = peerId + "|" + relativePath;
                std::vector<uint8_t> fullDelta;
                
                {
                    std::lock_guard<std::mutex> lock(pendingMutex_);
                    auto& pending = pendingDeltas_[key];

                    if (pending.totalChunks != totalChunks || pending.chunks.size() != totalChunks) {
                        pending.totalChunks = totalChunks;
                        pending.receivedChunks = 0;
                        pending.chunks.assign(totalChunks, {});
                    }
                    
                    // Update last activity timestamp
                    pending.lastActivity = std::chrono::steady_clock::now();

                    if (chunkId >= pending.totalChunks) {
                        logger.error("Delta chunkId out of range for " + filename, "DeltaSyncProtocol");
                        return;
                    }

                    std::vector<uint8_t> chunk(rawData.begin() + thirdPipe + 1, rawData.end());
                    if (pending.chunks[chunkId].empty()) {
                        pending.chunks[chunkId] = std::move(chunk);
                        pending.receivedChunks++;
                    }

                    if (pending.receivedChunks < pending.totalChunks) {
                        // Wait for more chunks
                        return;
                    }

                    // All chunks received: reassemble full DELTA_DATA message
                    for (std::uint32_t i = 0; i < pending.totalChunks; ++i) {
                        const auto& c = pending.chunks[i];
                        fullDelta.insert(fullDelta.end(), c.begin(), c.end());
                    }

                    pendingDeltas_.erase(key);
                }

                std::string header = "DELTA_DATA|" + relativePath + "|";
                std::vector<uint8_t> fullRaw(header.begin(), header.end());
                fullRaw.insert(fullRaw.end(), fullDelta.begin(), fullDelta.end());

                handleDeltaData(peerId, fullRaw);
                return;
            }
        }

        logger.info("Received delta data for: " + filename + " from " + peerId, "DeltaSyncProtocol");
        
        if (rawData.size() <= secondPipe + 1) {
            logger.error("No delta data in DELTA_DATA message", "DeltaSyncProtocol");
            return;
        }
        
        std::vector<uint8_t> deltaData(rawData.begin() + secondPipe + 1, rawData.end());
        auto deltas = DeltaSerialization::deserializeDelta(deltaData);
        
        logger.debug("Applying " + std::to_string(deltas.size()) + " delta instructions", "DeltaSyncProtocol");
        
        // Create parent directories if needed
        std::filesystem::path parentDir = std::filesystem::path(localPath).parent_path();
        if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
            std::filesystem::create_directories(parentDir);
        }
        
        // Create empty file if doesn't exist
        if (!std::filesystem::exists(localPath)) {
            logger.debug("Creating new file: " + filename, "DeltaSyncProtocol");
            std::ofstream outfile(localPath);
            if (!outfile) {
                logger.error("Failed to create file: " + localPath, "DeltaSyncProtocol");
                metrics.incrementSyncErrors();
                return;
            }
            outfile.close();
        }
        
        // Apply delta
        auto newData = DeltaEngine::applyDelta(localPath, deltas);
        
        // Mark as patched BEFORE writing to prevent sync loop
        if (markAsPatchedCallback_) {
            markAsPatchedCallback_(filename);
        }
        
        // Write updated file
        filesystem_->writeFile(localPath, newData);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        
        logger.info("Successfully patched file: " + filename + " (" + std::to_string(elapsed) + "ms)", "DeltaSyncProtocol");
        
        metrics.incrementDeltasReceived();
        metrics.incrementFilesSynced();
        metrics.addBytesDownloaded(rawData.size());
        metrics.recordSyncLatency(elapsed);
        metrics.incrementTransfersCompleted();
        
    } catch (const std::exception& e) {
        logger.error("Error in handleDeltaData: " + std::string(e.what()), "DeltaSyncProtocol");
        metrics.incrementSyncErrors();
        metrics.incrementTransfersFailed();
    }
}

} // namespace SentinelFS
