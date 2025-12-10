/**
 * @file FileTransferHandler.cpp
 * @brief Handle FILE_REQUEST, FILE_DATA, and DELETE_FILE messages
 */

#include "DeltaSyncProtocolHandler.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace SentinelFS {

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
        
        // Start transfer tracking
        std::string transferId = metrics.startTransfer(relativePath, peerId, true, totalSize);
        
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
                metrics.completeTransfer(transferId, false);
                return;
            }
            
            metrics.addBytesUploaded(payload.size());
            metrics.updateTransferProgress(transferId, offset + len);
        }
        
        logger.info("Sent file " + filename + " (" + std::to_string(totalSize) + " bytes) to " + peerId, "DeltaSyncProtocol");
        metrics.completeTransfer(transferId, true);
        
        // Log file access to database for history tracking
        if (storage_) {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            storage_->logFileAccess(localPath, "upload", peerId, timestamp);
        }
        
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
        
        std::string remotePath = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        std::string chunkInfo = msg.substr(secondPipe + 1, thirdPipe - secondPipe - 1);
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
        
        // Keep remotePath for key assembly
        std::string relativePath = remotePath;
        
        size_t slashPos = chunkInfo.find('/');
        if (slashPos == std::string::npos) {
            logger.error("Invalid chunk info in FILE_DATA", "DeltaSyncProtocol");
            return;
        }
        
        std::uint32_t chunkId = static_cast<std::uint32_t>(std::stoul(chunkInfo.substr(0, slashPos)));
        std::uint32_t totalChunks = static_cast<std::uint32_t>(std::stoul(chunkInfo.substr(slashPos + 1)));
        
        std::string key = peerId + "|FILE|" + relativePath;
        std::vector<uint8_t> fullFile;
        
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
            for (const auto& c : pending.chunks) {
                fullFile.insert(fullFile.end(), c.begin(), c.end());
            }
            pendingDeltas_.erase(key);
        }
        
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
        
        // Log file access to database for history tracking
        if (storage_) {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            storage_->logFileAccess(localPath, "download", peerId, timestamp);
        }
        
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
