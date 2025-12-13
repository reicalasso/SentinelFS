/**
 * @file DeltaRequestHandler.cpp
 * @brief Handle REQUEST_DELTA messages - compute and send delta
 */

#include "DeltaSyncProtocolHandler.h"
#include "DeltaSerialization.h"
#include "DeltaEngine.h"
#include "INetworkAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <chrono>

namespace SentinelFS {

void DeltaSyncProtocolHandler::handleDeltaRequest(const std::string& peerId,
                                                  const std::vector<uint8_t>& rawData) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        std::string msg(rawData.begin(), rawData.end());
        
        size_t firstPipe = msg.find('|');
        size_t secondPipe = msg.find('|', firstPipe + 1);
        
        if (secondPipe == std::string::npos) {
            logger.error("Invalid REQUEST_DELTA message format", "DeltaSyncProtocol");
            return;
        }
        
        // This is the path from peer (might be relative or absolute)
        std::string remotePath = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        std::string filename = std::filesystem::path(remotePath).filename().string();
        
        // Convert to local absolute path
        std::string localPath;
        if (!remotePath.empty() && remotePath[0] == '/') {
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
            localPath += remotePath;
        }
        
        logger.info("Received delta request for: " + filename + " from " + peerId, "DeltaSyncProtocol");
        
        if (rawData.size() <= secondPipe + 1) {
            logger.error("No signature data in REQUEST_DELTA", "DeltaSyncProtocol");
            return;
        }
        
        std::vector<uint8_t> sigData(rawData.begin() + secondPipe + 1, rawData.end());
        auto sigs = DeltaSerialization::deserializeSignature(sigData);
        
        if (!std::filesystem::exists(localPath)) {
            logger.warn("File not found locally: " + filename + " at " + localPath, "DeltaSyncProtocol");
            return;
        }
        
        // Calculate delta
        logger.debug("Calculating delta for: " + filename, "DeltaSyncProtocol");
        size_t blockSize = DeltaEngine::getAdaptiveBlockSize(localPath, std::filesystem::file_size(localPath));
        auto deltas = DeltaEngine::calculateDelta(localPath, sigs);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        metrics.recordDeltaComputeTime(elapsed);
        
        auto serializedDelta = DeltaSerialization::serializeDelta(deltas, blockSize);

        // Chunk-based transfer for large deltas
        const std::size_t CHUNK_SIZE = 64 * 1024; // 64KB per chunk
        std::size_t totalSize = serializedDelta.size();
        std::uint32_t totalChunks = totalSize == 0
            ? 0
            : static_cast<std::uint32_t>((totalSize + CHUNK_SIZE - 1) / CHUNK_SIZE);

        if (totalChunks == 0) {
            // No delta data, but send an empty payload for protocol symmetry
            std::string header = "DELTA_DATA|" + remotePath + "|";
            std::vector<uint8_t> payload(header.begin(), header.end());
            bool sent = network_->sendData(peerId, payload);
            if (!sent) {
                logger.error("Failed to send empty delta data to " + peerId, "DeltaSyncProtocol");
                metrics.incrementTransfersFailed();
            }
            return;
        }

        bool allSent = true;
        for (std::uint32_t chunkId = 0; chunkId < totalChunks; ++chunkId) {
            std::size_t offset = static_cast<std::size_t>(chunkId) * CHUNK_SIZE;
            std::size_t len = std::min(CHUNK_SIZE, totalSize - offset);

            std::string header = "DELTA_DATA|" + remotePath + "|" +
                                 std::to_string(chunkId) + "/" + std::to_string(totalChunks) + "|";
            std::vector<uint8_t> payload(header.begin(), header.end());
            payload.insert(payload.end(),
                          serializedDelta.begin() + static_cast<std::ptrdiff_t>(offset),
                          serializedDelta.begin() + static_cast<std::ptrdiff_t>(offset + len));

            bool sent = network_->sendData(peerId, payload);
            if (!sent) {
                logger.error("Failed to send delta chunk " + std::to_string(chunkId) +
                             " of " + std::to_string(totalChunks) + " for " + filename, "DeltaSyncProtocol");
                metrics.incrementTransfersFailed();
                allSent = false;
                break;
            }

            metrics.addBytesUploaded(payload.size());
        }

        if (allSent) {
            logger.info("Sent delta with " + std::to_string(deltas.size()) + " instructions in " +
                        std::to_string(totalChunks) + " chunks to " + peerId, "DeltaSyncProtocol");
            metrics.incrementDeltasSent();
        }

    } catch (const std::exception& e) {
        logger.error("Error in handleDeltaRequest: " + std::string(e.what()), "DeltaSyncProtocol");
        metrics.incrementSyncErrors();
    }
}

} // namespace SentinelFS
