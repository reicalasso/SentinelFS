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
        
        // Convert relative path to local absolute path
        std::string localPath = watchDirectory_;
        if (!localPath.empty() && localPath.back() != '/') {
            localPath += '/';
        }
        localPath += relativePath;
        
        std::string filename = std::filesystem::path(relativePath).filename().string();
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
        
        // This is the relative path from peer
        std::string relativePath = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        std::string filename = std::filesystem::path(relativePath).filename().string();
        
        // Convert to local absolute path
        std::string localPath = watchDirectory_;
        if (!localPath.empty() && localPath.back() != '/') {
            localPath += '/';
        }
        localPath += relativePath;
        
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
            std::string header = "DELTA_DATA|" + relativePath + "|";
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

            std::string header = "DELTA_DATA|" + relativePath + "|" +
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
        
        // This is the relative path
        std::string relativePath = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        std::string filename = std::filesystem::path(relativePath).filename().string();
        
        // Convert to local absolute path
        std::string localPath = watchDirectory_;
        if (!localPath.empty() && localPath.back() != '/') {
            localPath += '/';
        }
        localPath += relativePath;
        
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

void DeltaSyncProtocolHandler::handleFileRequest(const std::string& peerId,
                                                  const std::vector<uint8_t>& rawData) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    try {
        std::string msg(rawData.begin(), rawData.end());
        
        // Parse: REQUEST_FILE|relativePath
        const std::string prefix = "REQUEST_FILE|";
        if (msg.size() <= prefix.length()) {
            logger.error("Invalid REQUEST_FILE message format", "DeltaSyncProtocol");
            return;
        }
        
        std::string relativePath = msg.substr(prefix.length());
        std::string filename = std::filesystem::path(relativePath).filename().string();
        
        // Convert to local absolute path
        std::string localPath = watchDirectory_;
        if (!localPath.empty() && localPath.back() != '/') {
            localPath += '/';
        }
        localPath += relativePath;
        
        logger.info("Received file request for: " + filename + " from " + peerId, "DeltaSyncProtocol");
        
        if (!std::filesystem::exists(localPath)) {
            logger.warn("Requested file not found: " + localPath, "DeltaSyncProtocol");
            return;
        }
        
        // Read file content
        std::ifstream file(localPath, std::ios::binary);
        if (!file) {
            logger.error("Failed to open file: " + localPath, "DeltaSyncProtocol");
            return;
        }
        
        std::vector<uint8_t> fileContent((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
        file.close();
        
        // Send file in chunks with relative path
        const std::size_t CHUNK_SIZE = 64 * 1024; // 64KB
        std::size_t totalSize = fileContent.size();
        std::uint32_t totalChunks = totalSize == 0 ? 1 : 
            static_cast<std::uint32_t>((totalSize + CHUNK_SIZE - 1) / CHUNK_SIZE);
        
        for (std::uint32_t chunkId = 0; chunkId < totalChunks; ++chunkId) {
            std::size_t offset = static_cast<std::size_t>(chunkId) * CHUNK_SIZE;
            std::size_t len = std::min(CHUNK_SIZE, totalSize - offset);
            
            std::string header = "FILE_DATA|" + relativePath + "|" +
                                 std::to_string(chunkId) + "/" + std::to_string(totalChunks) + "|";
            std::vector<uint8_t> payload(header.begin(), header.end());
            
            if (len > 0) {
                payload.insert(payload.end(),
                              fileContent.begin() + static_cast<std::ptrdiff_t>(offset),
                              fileContent.begin() + static_cast<std::ptrdiff_t>(offset + len));
            }
            
            if (!network_->sendData(peerId, payload)) {
                logger.error("Failed to send file chunk " + std::to_string(chunkId), "DeltaSyncProtocol");
                metrics.incrementTransfersFailed();
                return;
            }
            
            metrics.addBytesUploaded(payload.size());
        }
        
        logger.info("Sent file " + filename + " (" + std::to_string(totalSize) + " bytes) to " + peerId, "DeltaSyncProtocol");
        metrics.incrementTransfersCompleted();
        
    } catch (const std::exception& e) {
        logger.error("Error in handleFileRequest: " + std::string(e.what()), "DeltaSyncProtocol");
        metrics.incrementSyncErrors();
    }
}

void DeltaSyncProtocolHandler::handleFileData(const std::string& peerId,
                                               const std::vector<uint8_t>& rawData) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    try {
        std::string msg(rawData.begin(), rawData.end());
        
        // Parse: FILE_DATA|relativePath|chunkId/total|data
        size_t firstPipe = msg.find('|');
        size_t secondPipe = msg.find('|', firstPipe + 1);
        size_t thirdPipe = msg.find('|', secondPipe + 1);
        
        if (thirdPipe == std::string::npos) {
            logger.error("Invalid FILE_DATA message format", "DeltaSyncProtocol");
            return;
        }
        
        std::string relativePath = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        std::string chunkInfo = msg.substr(secondPipe + 1, thirdPipe - secondPipe - 1);
        std::string filename = std::filesystem::path(relativePath).filename().string();
        
        // Convert to local absolute path
        std::string localPath = watchDirectory_;
        if (!localPath.empty() && localPath.back() != '/') {
            localPath += '/';
        }
        localPath += relativePath;
        
        size_t slashPos = chunkInfo.find('/');
        if (slashPos == std::string::npos) {
            logger.error("Invalid chunk info in FILE_DATA", "DeltaSyncProtocol");
            return;
        }
        
        std::uint32_t chunkId = static_cast<std::uint32_t>(std::stoul(chunkInfo.substr(0, slashPos)));
        std::uint32_t totalChunks = static_cast<std::uint32_t>(std::stoul(chunkInfo.substr(slashPos + 1)));
        
        std::string key = peerId + "|FILE|" + relativePath;
        auto& pending = pendingDeltas_[key];
        
        if (pending.totalChunks != totalChunks || pending.chunks.size() != totalChunks) {
            pending.totalChunks = totalChunks;
            pending.receivedChunks = 0;
            pending.chunks.assign(totalChunks, {});
        }
        
        if (chunkId >= pending.totalChunks) {
            logger.error("File chunk ID out of range", "DeltaSyncProtocol");
            return;
        }
        
        std::vector<uint8_t> chunk(rawData.begin() + thirdPipe + 1, rawData.end());
        if (pending.chunks[chunkId].empty()) {
            pending.chunks[chunkId] = std::move(chunk);
            pending.receivedChunks++;
        }
        
        if (pending.receivedChunks < pending.totalChunks) {
            return; // Wait for more chunks
        }
        
        // All chunks received - reassemble file
        std::vector<uint8_t> fullFile;
        for (const auto& c : pending.chunks) {
            fullFile.insert(fullFile.end(), c.begin(), c.end());
        }
        pendingDeltas_.erase(key);
        
        // Mark as patched before writing
        if (markAsPatchedCallback_) {
            markAsPatchedCallback_(filename);
        }
        
        // Create parent directories if needed
        std::filesystem::path parentDir = std::filesystem::path(localPath).parent_path();
        if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
            std::filesystem::create_directories(parentDir);
        }
        
        // Write file to local path
        filesystem_->writeFile(localPath, fullFile);
        
        logger.info("Received file " + filename + " (" + std::to_string(fullFile.size()) + " bytes) from " + peerId, "DeltaSyncProtocol");
        metrics.incrementFilesSynced();
        metrics.addBytesDownloaded(fullFile.size());
        metrics.incrementTransfersCompleted();
        
    } catch (const std::exception& e) {
        logger.error("Error in handleFileData: " + std::string(e.what()), "DeltaSyncProtocol");
        metrics.incrementSyncErrors();
        metrics.incrementTransfersFailed();
    }
}

void DeltaSyncProtocolHandler::handleDeleteFile(const std::string& peerId,
                                                 const std::vector<uint8_t>& rawData) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    try {
        std::string msg(rawData.begin(), rawData.end());
        
        // Parse: DELETE_FILE|relativePath
        const std::string prefix = "DELETE_FILE|";
        if (msg.size() <= prefix.length()) {
            logger.error("Invalid DELETE_FILE message format", "DeltaSyncProtocol");
            return;
        }
        
        std::string relativePath = msg.substr(prefix.length());
        std::string filename = std::filesystem::path(relativePath).filename().string();
        
        // Convert to local absolute path
        std::string localPath = watchDirectory_;
        if (!localPath.empty() && localPath.back() != '/') {
            localPath += '/';
        }
        localPath += relativePath;
        
        logger.info("Received delete request for: " + filename + " from " + peerId, "DeltaSyncProtocol");
        
        // Mark as patched to prevent sync loop
        if (markAsPatchedCallback_) {
            markAsPatchedCallback_(filename);
        }
        
        // Delete file if exists
        if (std::filesystem::exists(localPath)) {
            std::filesystem::remove(localPath);
            logger.info("Deleted file: " + filename, "DeltaSyncProtocol");
        }
        
        // Remove from database
        if (storage_) {
            storage_->removeFile(localPath);
        }
        
        metrics.incrementFilesSynced();
        
    } catch (const std::exception& e) {
        logger.error("Error in handleDeleteFile: " + std::string(e.what()), "DeltaSyncProtocol");
        metrics.incrementSyncErrors();
    }
}

} // namespace SentinelFS
