#include "DeltaSyncProtocolHandler.h"
#include "DeltaSerialization.h"
#include "DeltaEngine.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>

namespace SentinelFS {

DeltaSyncProtocolHandler::DeltaSyncProtocolHandler(INetworkAPI* network, IStorageAPI* storage,
                                                   IFileAPI* filesystem, const std::string& watchDir)
    : network_(network), storage_(storage), filesystem_(filesystem), watchDirectory_(watchDir) {
    auto& logger = Logger::instance();
    logger.debug("DeltaSyncProtocolHandler initialized for: " + watchDir, "DeltaSyncProtocol");
}

void DeltaSyncProtocolHandler::handleUpdateAvailable(const std::string& peerId, 
                                                     const std::vector<uint8_t>& rawData) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    try {
        std::string fullMsg(rawData.begin(), rawData.end());
        
        // Parse message
        const std::string prefix = "UPDATE_AVAILABLE|";
        if (fullMsg.size() <= prefix.length()) {
            logger.error("Invalid UPDATE_AVAILABLE message format", "DeltaSyncProtocol");
            return;
        }
        
        std::string filename = fullMsg.substr(prefix.length());
        logger.info("Peer " + peerId + " has update for: " + filename, "DeltaSyncProtocol");
        
        std::string localPath = watchDirectory_ + "/" + filename;
        
        // Calculate local signature
        std::vector<BlockSignature> sigs;
        if (std::filesystem::exists(localPath)) {
            logger.debug("Calculating signature for existing file: " + filename, "DeltaSyncProtocol");
            sigs = DeltaEngine::calculateSignature(localPath);
        } else {
            logger.debug("File doesn't exist locally, requesting full copy: " + filename, "DeltaSyncProtocol");
        }
        
        auto serializedSig = DeltaSerialization::serializeSignature(sigs);
        
        // Send delta request
        std::string header = "REQUEST_DELTA|" + filename + "|";
        std::vector<uint8_t> payload(header.begin(), header.end());
        payload.insert(payload.end(), serializedSig.begin(), serializedSig.end());
        
        bool sent = network_->sendData(peerId, payload);
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
        
        std::string filename = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        logger.info("Received delta request for: " + filename + " from " + peerId, "DeltaSyncProtocol");
        
        if (rawData.size() <= secondPipe + 1) {
            logger.error("No signature data in REQUEST_DELTA", "DeltaSyncProtocol");
            return;
        }
        
        std::vector<uint8_t> sigData(rawData.begin() + secondPipe + 1, rawData.end());
        auto sigs = DeltaSerialization::deserializeSignature(sigData);
        
        std::string localPath = watchDirectory_ + "/" + filename;
        if (!std::filesystem::exists(localPath)) {
            logger.warn("File not found locally: " + filename, "DeltaSyncProtocol");
            return;
        }
        
        // Calculate delta
        logger.debug("Calculating delta for: " + filename, "DeltaSyncProtocol");
        auto deltas = DeltaEngine::calculateDelta(localPath, sigs);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        metrics.recordDeltaComputeTime(elapsed);
        
        auto serializedDelta = DeltaSerialization::serializeDelta(deltas);

        // Chunk-based transfer for large deltas
        const std::size_t CHUNK_SIZE = 64 * 1024; // 64KB per chunk
        std::size_t totalSize = serializedDelta.size();
        std::uint32_t totalChunks = totalSize == 0
            ? 0
            : static_cast<std::uint32_t>((totalSize + CHUNK_SIZE - 1) / CHUNK_SIZE);

        if (totalChunks == 0) {
            // No delta data, but send an empty payload for protocol symmetry
            std::string header = "DELTA_DATA|" + filename + "|";
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

            std::string header = "DELTA_DATA|" + filename + "|" +
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
        
        std::string filename = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        
        // Check if this is a chunked DELTA_DATA message: DELTA_DATA|filename|chunkId/total|...
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

                std::string key = peerId + "|" + filename;
                auto& pending = pendingDeltas_[key];

                if (pending.totalChunks != totalChunks || pending.chunks.size() != totalChunks) {
                    pending.totalChunks = totalChunks;
                    pending.receivedChunks = 0;
                    pending.chunks.assign(totalChunks, {});
                }

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

                // All chunks received: reassemble full DELTA_DATA message and recurse once.
                std::vector<uint8_t> fullDelta;
                for (std::uint32_t i = 0; i < pending.totalChunks; ++i) {
                    const auto& c = pending.chunks[i];
                    fullDelta.insert(fullDelta.end(), c.begin(), c.end());
                }

                pendingDeltas_.erase(key);

                std::string header = "DELTA_DATA|" + filename + "|";
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
        
        std::string localPath = watchDirectory_ + "/" + filename;
        
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
