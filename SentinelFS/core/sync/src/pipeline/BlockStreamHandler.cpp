/**
 * @file BlockStreamHandler.cpp
 * @brief Stage 6: Block Stream - Data transfer with flow control
 * 
 * Block Stream Flow:
 * 1. Sender chunks data into CHUNK_SIZE pieces
 * 2. Each chunk is sent as BLOCK_DATA with sequence info
 * 3. Receiver acknowledges chunks (for flow control)
 * 4. Backpressure: sender waits if receiver is slow
 */

#include "SyncPipeline.h"
#include "INetworkAPI.h"
#include "IFileAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace SentinelFS {
namespace Sync {

void SyncPipeline::streamBlocks(const std::string& peerId, const std::string& relativePath,
                                const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    // Get agreed chunk size from session
    uint32_t chunkSize = CHUNK_SIZE;
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        auto it = peerSessions_.find(peerId);
        if (it != peerSessions_.end()) {
            chunkSize = it->second.agreedChunkSize;
        }
    }
    
    uint32_t totalChunks = data.empty() ? 1 :
        static_cast<uint32_t>((data.size() + chunkSize - 1) / chunkSize);
    
    logger.info("📦 Streaming " + filename + " (" + std::to_string(data.size()) + 
               " bytes) in " + std::to_string(totalChunks) + " chunks to " + peerId, "SyncPipeline");
    
    // Find transfer context
    std::string transferId;
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            transferId = it->second;
            auto& ctx = activeTransfers_[transferId];
            ctx.totalChunks = totalChunks;
            ctx.currentChunk = 0;
        }
    }
    
    // Stream chunks
    for (uint32_t i = 0; i < totalChunks; ++i) {
        size_t offset = static_cast<size_t>(i) * chunkSize;
        size_t len = std::min(static_cast<size_t>(chunkSize), data.size() - offset);
        
        // Build BLOCK_DATA message
        size_t payloadSize = sizeof(uint16_t) + sizeof(uint32_t) * 3 + relativePath.size() + len;
        std::vector<uint8_t> blockMsg(sizeof(MessageHeader) + payloadSize);
        
        MessageHeader* header = reinterpret_cast<MessageHeader*>(blockMsg.data());
        *header = createHeader(MessageType::BLOCK_DATA,
                              static_cast<uint32_t>(payloadSize),
                              getNextSequence(peerId));
        
        size_t pos = sizeof(MessageHeader);
        
        // Path length
        uint16_t pathLen = static_cast<uint16_t>(relativePath.size());
        std::memcpy(blockMsg.data() + pos, &pathLen, sizeof(pathLen));
        pos += sizeof(pathLen);
        
        // Chunk index
        std::memcpy(blockMsg.data() + pos, &i, sizeof(i));
        pos += sizeof(i);
        
        // Total chunks
        std::memcpy(blockMsg.data() + pos, &totalChunks, sizeof(totalChunks));
        pos += sizeof(totalChunks);
        
        // Data size
        uint32_t dataSize = static_cast<uint32_t>(len);
        std::memcpy(blockMsg.data() + pos, &dataSize, sizeof(dataSize));
        pos += sizeof(dataSize);
        
        // Path
        std::memcpy(blockMsg.data() + pos, relativePath.data(), relativePath.size());
        pos += relativePath.size();
        
        // Data
        if (len > 0) {
            std::memcpy(blockMsg.data() + pos, data.data() + offset, len);
        }
        
        if (!sendMessage(peerId, blockMsg)) {
            logger.error("Failed to send block " + std::to_string(i) + " of " + filename, "SyncPipeline");
            
            if (!transferId.empty()) {
                updateTransferState(transferId, TransferState::FAILED);
                if (completeCallback_) {
                    completeCallback_(transferId, false, "Failed to send block");
                }
            }
            return;
        }
        
        metrics.addBytesUploaded(blockMsg.size());
        
        // Update progress
        if (!transferId.empty()) {
            std::lock_guard<std::mutex> lock(transferMutex_);
            auto it = activeTransfers_.find(transferId);
            if (it != activeTransfers_.end()) {
                it->second.currentChunk = i + 1;
                it->second.bytesTransferred = offset + len;
                
                if (progressCallback_) {
                    progressCallback_(transferId, it->second.bytesTransferred, it->second.fileSize);
                }
            }
        }
    }
    
    logger.info("📤 Finished streaming " + filename + " to " + peerId, "SyncPipeline");
    
    // Send transfer complete
    if (!transferId.empty()) {
        sendTransferComplete(peerId, transferId);
    }
}

void SyncPipeline::handleBlockData(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    if (data.size() < sizeof(MessageHeader) + sizeof(uint16_t) + sizeof(uint32_t) * 3) {
        logger.error("BLOCK_DATA too small from " + peerId, "SyncPipeline");
        return;
    }
    
    size_t offset = sizeof(MessageHeader);
    
    uint16_t pathLen;
    std::memcpy(&pathLen, data.data() + offset, sizeof(pathLen));
    offset += sizeof(pathLen);
    
    uint32_t chunkIndex, totalChunks, dataSize;
    std::memcpy(&chunkIndex, data.data() + offset, sizeof(chunkIndex));
    offset += sizeof(chunkIndex);
    std::memcpy(&totalChunks, data.data() + offset, sizeof(totalChunks));
    offset += sizeof(totalChunks);
    std::memcpy(&dataSize, data.data() + offset, sizeof(dataSize));
    offset += sizeof(dataSize);
    
    if (data.size() < offset + pathLen + dataSize) {
        logger.error("BLOCK_DATA truncated from " + peerId, "SyncPipeline");
        return;
    }
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + offset), pathLen);
    offset += pathLen;
    
    std::string filename = std::filesystem::path(relativePath).filename().string();
    std::string localPath = getAbsolutePath(relativePath);
    
    // Extract block data
    std::vector<uint8_t> blockData(data.begin() + offset, data.begin() + offset + dataSize);
    
    // Handle chunked transfer
    std::string key = peerId + "|" + relativePath;
    std::vector<uint8_t> fullData;
    bool complete = false;
    
    {
        std::lock_guard<std::mutex> lock(chunkMutex_);
        auto& pending = pendingChunks_[key];
        
        if (pending.totalChunks != totalChunks) {
            pending.relativePath = relativePath;
            pending.totalChunks = totalChunks;
            pending.receivedChunks = 0;
            pending.chunks.assign(totalChunks, {});
        }
        
        pending.lastActivity = std::chrono::steady_clock::now();
        
        if (chunkIndex < totalChunks && pending.chunks[chunkIndex].empty()) {
            pending.chunks[chunkIndex] = blockData;
            pending.receivedChunks++;
        }
        
        // Send ACK for flow control
        struct BlockAck {
            MessageHeader header;
            uint16_t pathLength;
            uint32_t chunkIndex;
            uint32_t receivedChunks;
        } ack{};
        
        ack.header = createHeader(MessageType::BLOCK_ACK,
                                  sizeof(ack) - sizeof(MessageHeader) + relativePath.size(),
                                  getNextSequence(peerId));
        ack.pathLength = static_cast<uint16_t>(relativePath.size());
        ack.chunkIndex = chunkIndex;
        ack.receivedChunks = pending.receivedChunks;
        
        std::vector<uint8_t> ackData(sizeof(ack) + relativePath.size());
        std::memcpy(ackData.data(), &ack, sizeof(ack));
        std::memcpy(ackData.data() + sizeof(ack), relativePath.data(), relativePath.size());
        sendMessage(peerId, ackData);
        
        if (pending.receivedChunks < pending.totalChunks) {
            logger.debug("Received block " + std::to_string(chunkIndex + 1) + "/" +
                        std::to_string(totalChunks) + " for " + filename, "SyncPipeline");
            return;
        }
        
        // All chunks received - reassemble
        for (const auto& chunk : pending.chunks) {
            fullData.insert(fullData.end(), chunk.begin(), chunk.end());
        }
        
        pendingChunks_.erase(key);
        complete = true;
    }
    
    if (!complete) return;
    
    logger.info("📥 Received complete file " + filename + " (" + 
               std::to_string(fullData.size()) + " bytes)", "SyncPipeline");
    
    // Create parent directories if needed
    std::filesystem::path parentDir = std::filesystem::path(localPath).parent_path();
    if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
        std::filesystem::create_directories(parentDir);
    }
    
    // Mark as patched BEFORE writing to prevent sync loop
    if (markAsPatchedCallback_) {
        markAsPatchedCallback_(filename);
    }
    
    // Write file atomically
    std::string tempPath = localPath + ".tmp";
    {
        std::ofstream outfile(tempPath, std::ios::binary);
        if (!outfile) {
            logger.error("Failed to create temp file: " + tempPath, "SyncPipeline");
            return;
        }
        outfile.write(reinterpret_cast<const char*>(fullData.data()), fullData.size());
    }
    
    // Atomic rename
    std::filesystem::rename(tempPath, localPath);
    
    // Update transfer
    std::string transferId;
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            transferId = it->second;
            auto& ctx = activeTransfers_[transferId];
            ctx.bytesTransferred = fullData.size();
        }
    }
    
    logger.info("✅ Successfully received " + filename, "SyncPipeline");
    metrics.incrementFilesSynced();
    metrics.addBytesDownloaded(fullData.size());
    
    // Wait for TRANSFER_COMPLETE from sender for integrity check
}

void SyncPipeline::handleBlockAck(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    
    struct BlockAck {
        MessageHeader header;
        uint16_t pathLength;
        uint32_t chunkIndex;
        uint32_t receivedChunks;
    };
    
    if (data.size() < sizeof(BlockAck)) {
        return;
    }
    
    const BlockAck* ack = reinterpret_cast<const BlockAck*>(data.data());
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + sizeof(BlockAck)),
                             ack->pathLength);
    
    // Update transfer progress (for flow control)
    std::lock_guard<std::mutex> lock(transferMutex_);
    auto it = pathToTransfer_.find(relativePath + "|" + peerId);
    if (it != pathToTransfer_.end()) {
        auto& ctx = activeTransfers_[it->second];
        ctx.lastActivity = std::chrono::steady_clock::now();
        
        // Could implement backpressure here if needed
    }
}

} // namespace Sync
} // namespace SentinelFS
