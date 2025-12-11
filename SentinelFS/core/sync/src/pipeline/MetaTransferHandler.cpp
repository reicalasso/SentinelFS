/**
 * @file MetaTransferHandler.cpp
 * @brief Stage 3: Meta Transfer - File metadata exchange
 * 
 * Meta Transfer Flow:
 * 1. Sender sends FILE_META with file info (size, hash, mtime, permissions)
 * 2. Receiver checks if file exists locally
 * 3. Receiver sends FILE_META_ACK with decision:
 *    - NEED_FULL: File doesn't exist, request full transfer
 *    - NEED_DELTA: File exists, request delta sync
 *    - UP_TO_DATE: Same hash, no transfer needed
 */

#include "SyncPipeline.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace SentinelFS {
namespace Sync {

// FILE_META_ACK response types
enum class MetaAckType : uint8_t {
    UP_TO_DATE = 0,     // Same hash, no transfer needed
    NEED_DELTA = 1,     // File exists, request delta
    NEED_FULL = 2       // File doesn't exist, request full
};

std::string SyncPipeline::sendFileMeta(const std::string& peerId, const std::string& localPath) {
    auto& logger = Logger::instance();
    
    if (!std::filesystem::exists(localPath)) {
        logger.error("File not found: " + localPath, "SyncPipeline");
        return "";
    }
    
    std::string relativePath = getRelativePath(localPath);
    std::string filename = std::filesystem::path(localPath).filename().string();
    
    // Check for existing transfer
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            logger.debug("Transfer already in progress for " + filename, "SyncPipeline");
            return it->second;
        }
    }
    
    // Get file info
    uint64_t fileSize = std::filesystem::file_size(localPath);
    auto mtime = std::filesystem::last_write_time(localPath);
    auto mtimeEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        mtime.time_since_epoch()).count();
    
    std::vector<uint8_t> hash = calculateFileHash(localPath);
    
    // Create transfer context
    std::string transferId = generateTransferId();
    
    TransferContext ctx;
    ctx.transferId = transferId;
    ctx.peerId = peerId;
    ctx.relativePath = relativePath;
    ctx.localPath = localPath;
    ctx.state = TransferState::SENDING_META;
    ctx.fileSize = fileSize;
    ctx.fileHash = hash;
    ctx.startTime = std::chrono::steady_clock::now();
    ctx.lastActivity = ctx.startTime;
    
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        activeTransfers_[transferId] = ctx;
        pathToTransfer_[relativePath + "|" + peerId] = transferId;
    }
    
    // Build FILE_META message
    size_t pathLen = relativePath.size();
    size_t payloadSize = sizeof(FileMeta) - sizeof(MessageHeader) + pathLen;
    
    std::vector<uint8_t> msgData(sizeof(FileMeta) + pathLen);
    FileMeta* meta = reinterpret_cast<FileMeta*>(msgData.data());
    
    meta->header = createHeader(MessageType::FILE_META, 
                                static_cast<uint32_t>(payloadSize),
                                getNextSequence(peerId));
    meta->fileSize = fileSize;
    meta->modTime = static_cast<uint64_t>(mtimeEpoch);
    meta->permissions = static_cast<uint32_t>(std::filesystem::status(localPath).permissions());
    meta->fileType = 0;  // Regular file
    meta->hashType = 0;  // SHA-256
    std::memcpy(meta->fileHash, hash.data(), std::min(hash.size(), sizeof(meta->fileHash)));
    meta->pathLength = static_cast<uint16_t>(pathLen);
    
    // Append path
    std::memcpy(msgData.data() + sizeof(FileMeta), relativePath.data(), pathLen);
    
    // Send
    if (!sendMessage(peerId, msgData)) {
        logger.error("Failed to send FILE_META for " + filename, "SyncPipeline");
        updateTransferState(transferId, TransferState::FAILED);
        return "";
    }
    
    logger.info("📋 Sent FILE_META for " + filename + " (" + std::to_string(fileSize) + " bytes) to " + peerId, "SyncPipeline");
    updateTransferState(transferId, TransferState::AWAITING_META_ACK);
    
    return transferId;
}

void SyncPipeline::handleFileMeta(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    if (data.size() < sizeof(FileMeta)) {
        logger.error("FILE_META too small from " + peerId, "SyncPipeline");
        return;
    }
    
    const FileMeta* meta = reinterpret_cast<const FileMeta*>(data.data());
    
    // Extract path
    if (data.size() < sizeof(FileMeta) + meta->pathLength) {
        logger.error("FILE_META path truncated from " + peerId, "SyncPipeline");
        return;
    }
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + sizeof(FileMeta)),
                             meta->pathLength);
    std::string localPath = getAbsolutePath(relativePath);
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    logger.info("📋 Received FILE_META for " + filename + " (" + 
               std::to_string(meta->fileSize) + " bytes) from " + peerId, "SyncPipeline");
    
    // Determine response
    MetaAckType ackType;
    std::vector<uint8_t> localHash;
    
    if (!std::filesystem::exists(localPath)) {
        // File doesn't exist - need full transfer
        ackType = MetaAckType::NEED_FULL;
        logger.debug("File doesn't exist locally, requesting full transfer", "SyncPipeline");
    } else {
        // File exists - check hash
        localHash = calculateFileHash(localPath);
        
        bool sameHash = (localHash.size() >= 32 && 
                        std::memcmp(localHash.data(), meta->fileHash, 32) == 0);
        
        if (sameHash) {
            ackType = MetaAckType::UP_TO_DATE;
            logger.info("✅ File " + filename + " is up to date", "SyncPipeline");
        } else {
            // Check if delta sync is supported
            std::lock_guard<std::mutex> lock(sessionMutex_);
            auto it = peerSessions_.find(peerId);
            bool deltaSupported = (it != peerSessions_.end() && 
                                  hasCapability(it->second.negotiatedCaps, Capability::DELTA_SYNC));
            
            if (deltaSupported) {
                ackType = MetaAckType::NEED_DELTA;
                logger.debug("File exists with different hash, requesting delta", "SyncPipeline");
            } else {
                ackType = MetaAckType::NEED_FULL;
                logger.debug("Delta not supported, requesting full transfer", "SyncPipeline");
            }
        }
    }
    
    // Build FILE_META_ACK
    struct FileMetaAck {
        MessageHeader header;
        uint8_t ackType;
        uint8_t localHash[32];
        uint16_t pathLength;
    } ack{};
    
    ack.header = createHeader(MessageType::FILE_META_ACK,
                              sizeof(ack) - sizeof(MessageHeader) + relativePath.size(),
                              getNextSequence(peerId));
    ack.ackType = static_cast<uint8_t>(ackType);
    if (!localHash.empty()) {
        std::memcpy(ack.localHash, localHash.data(), std::min(localHash.size(), sizeof(ack.localHash)));
    }
    ack.pathLength = static_cast<uint16_t>(relativePath.size());
    
    std::vector<uint8_t> ackData(sizeof(ack) + relativePath.size());
    std::memcpy(ackData.data(), &ack, sizeof(ack));
    std::memcpy(ackData.data() + sizeof(ack), relativePath.data(), relativePath.size());
    
    if (!sendMessage(peerId, ackData)) {
        logger.error("Failed to send FILE_META_ACK for " + filename, "SyncPipeline");
        return;
    }
    
    // If we need data, create a receive context
    if (ackType != MetaAckType::UP_TO_DATE) {
        std::string transferId = generateTransferId();
        
        TransferContext ctx;
        ctx.transferId = transferId;
        ctx.peerId = peerId;
        ctx.relativePath = relativePath;
        ctx.localPath = localPath;
        ctx.state = (ackType == MetaAckType::NEED_DELTA) ? 
                    TransferState::COMPUTING_SIGNATURE : TransferState::STREAMING_BLOCKS;
        ctx.fileSize = meta->fileSize;
        ctx.fileHash.assign(meta->fileHash, meta->fileHash + 32);
        ctx.useDelta = (ackType == MetaAckType::NEED_DELTA);
        ctx.startTime = std::chrono::steady_clock::now();
        ctx.lastActivity = ctx.startTime;
        
        std::lock_guard<std::mutex> lock(transferMutex_);
        activeTransfers_[transferId] = ctx;
        pathToTransfer_[relativePath + "|" + peerId] = transferId;
    }
    
    metrics.addBytesDownloaded(data.size());
}

void SyncPipeline::handleFileMetaAck(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    
    struct FileMetaAck {
        MessageHeader header;
        uint8_t ackType;
        uint8_t localHash[32];
        uint16_t pathLength;
    };
    
    if (data.size() < sizeof(FileMetaAck)) {
        logger.error("FILE_META_ACK too small from " + peerId, "SyncPipeline");
        return;
    }
    
    const FileMetaAck* ack = reinterpret_cast<const FileMetaAck*>(data.data());
    
    // Extract path
    std::string relativePath(reinterpret_cast<const char*>(data.data() + sizeof(FileMetaAck)),
                             ack->pathLength);
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    MetaAckType ackType = static_cast<MetaAckType>(ack->ackType);
    
    // Find transfer
    std::string transferId;
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            transferId = it->second;
        }
    }
    
    if (transferId.empty()) {
        logger.warn("No transfer found for FILE_META_ACK: " + filename, "SyncPipeline");
        return;
    }
    
    switch (ackType) {
        case MetaAckType::UP_TO_DATE:
            logger.info("✅ Peer " + peerId + " already has " + filename, "SyncPipeline");
            updateTransferState(transferId, TransferState::COMPLETE);
            
            if (completeCallback_) {
                completeCallback_(transferId, true, "");
            }
            break;
            
        case MetaAckType::NEED_DELTA:
            logger.info("🔄 Peer " + peerId + " requests delta for " + filename, "SyncPipeline");
            updateTransferState(transferId, TransferState::COMPUTING_SIGNATURE);
            
            // Wait for signature request from peer
            break;
            
        case MetaAckType::NEED_FULL:
            logger.info("📦 Peer " + peerId + " requests full transfer for " + filename, "SyncPipeline");
            updateTransferState(transferId, TransferState::STREAMING_BLOCKS);
            
            // Start streaming blocks
            {
                std::string localPath;
                {
                    std::lock_guard<std::mutex> lock(transferMutex_);
                    auto it = activeTransfers_.find(transferId);
                    if (it != activeTransfers_.end()) {
                        localPath = it->second.localPath;
                    }
                }
                
                if (!localPath.empty()) {
                    // Read file directly
                    std::vector<uint8_t> fileData;
                    std::ifstream file(localPath, std::ios::binary);
                    if (file) {
                        fileData = std::vector<uint8_t>(
                            (std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
                    }
                    if (!fileData.empty()) {
                        streamBlocks(peerId, relativePath, fileData);
                    }
                }
            }
            break;
    }
}

} // namespace Sync
} // namespace SentinelFS
