/**
 * @file DeltaSyncHandler.cpp
 * @brief Stage 4 & 5: Hash Scan + Delta Request
 * 
 * Delta Sync Flow:
 * 1. Receiver computes signature of local file (rolling hash + strong hash per block)
 * 2. Receiver sends SIGNATURE_RESPONSE to sender
 * 3. Sender computes delta using sliding window algorithm
 * 4. Sender sends DELTA_RESPONSE with COPY/LITERAL instructions
 * 5. Receiver applies delta to reconstruct new file
 */

#include "SyncPipeline.h"
#include "DeltaEngine.h"
#include "DeltaSerialization.h"
#include "INetworkAPI.h"
#include "IFileAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <cstring>

namespace SentinelFS {
namespace Sync {

void SyncPipeline::requestSignature(const std::string& peerId, const std::string& relativePath) {
    auto& logger = Logger::instance();
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    logger.debug("Requesting signature for " + filename + " from " + peerId, "SyncPipeline");
    
    // Build SIGNATURE_REQUEST
    struct SignatureRequest {
        MessageHeader header;
        uint16_t pathLength;
    } req{};
    
    req.header = createHeader(MessageType::SIGNATURE_REQUEST,
                              sizeof(req) - sizeof(MessageHeader) + relativePath.size(),
                              getNextSequence(peerId));
    req.pathLength = static_cast<uint16_t>(relativePath.size());
    
    std::vector<uint8_t> reqData(sizeof(req) + relativePath.size());
    std::memcpy(reqData.data(), &req, sizeof(req));
    std::memcpy(reqData.data() + sizeof(req), relativePath.data(), relativePath.size());
    
    sendMessage(peerId, reqData);
}

void SyncPipeline::handleSignatureRequest(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    struct SignatureRequest {
        MessageHeader header;
        uint16_t pathLength;
    };
    
    if (data.size() < sizeof(SignatureRequest)) {
        logger.error("SIGNATURE_REQUEST too small from " + peerId, "SyncPipeline");
        return;
    }
    
    const SignatureRequest* req = reinterpret_cast<const SignatureRequest*>(data.data());
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + sizeof(SignatureRequest)),
                             req->pathLength);
    std::string localPath = getAbsolutePath(relativePath);
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    logger.info("🔍 Computing signature for " + filename, "SyncPipeline");
    
    // Compute signature using DeltaEngine
    std::vector<BlockSignature> sigs;
    if (std::filesystem::exists(localPath)) {
        sigs = DeltaEngine::calculateSignature(localPath);
    }
    
    logger.debug("Computed " + std::to_string(sigs.size()) + " block signatures", "SyncPipeline");
    
    // Serialize signatures
    auto serializedSigs = DeltaSerialization::serializeSignature(sigs);
    
    // Build SIGNATURE_RESPONSE
    size_t payloadSize = sizeof(uint16_t) + sizeof(uint32_t) + relativePath.size() + serializedSigs.size();
    
    std::vector<uint8_t> respData(sizeof(MessageHeader) + payloadSize);
    
    MessageHeader* header = reinterpret_cast<MessageHeader*>(respData.data());
    *header = createHeader(MessageType::SIGNATURE_RESPONSE,
                          static_cast<uint32_t>(payloadSize),
                          getNextSequence(peerId));
    
    size_t offset = sizeof(MessageHeader);
    
    // Path length
    uint16_t pathLen = static_cast<uint16_t>(relativePath.size());
    std::memcpy(respData.data() + offset, &pathLen, sizeof(pathLen));
    offset += sizeof(pathLen);
    
    // Block count
    uint32_t blockCount = static_cast<uint32_t>(sigs.size());
    std::memcpy(respData.data() + offset, &blockCount, sizeof(blockCount));
    offset += sizeof(blockCount);
    
    // Path
    std::memcpy(respData.data() + offset, relativePath.data(), relativePath.size());
    offset += relativePath.size();
    
    // Signatures
    std::memcpy(respData.data() + offset, serializedSigs.data(), serializedSigs.size());
    
    if (!sendMessage(peerId, respData)) {
        logger.error("Failed to send SIGNATURE_RESPONSE for " + filename, "SyncPipeline");
        return;
    }
    
    logger.info("📤 Sent signature (" + std::to_string(sigs.size()) + " blocks) for " + filename, "SyncPipeline");
    metrics.addBytesUploaded(respData.size());
}

void SyncPipeline::handleSignatureResponse(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    
    if (data.size() < sizeof(MessageHeader) + sizeof(uint16_t) + sizeof(uint32_t)) {
        logger.error("SIGNATURE_RESPONSE too small from " + peerId, "SyncPipeline");
        return;
    }
    
    size_t offset = sizeof(MessageHeader);
    
    uint16_t pathLen;
    std::memcpy(&pathLen, data.data() + offset, sizeof(pathLen));
    offset += sizeof(pathLen);
    
    uint32_t blockCount;
    std::memcpy(&blockCount, data.data() + offset, sizeof(blockCount));
    offset += sizeof(blockCount);
    
    if (data.size() < offset + pathLen) {
        logger.error("SIGNATURE_RESPONSE path truncated", "SyncPipeline");
        return;
    }
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + offset), pathLen);
    offset += pathLen;
    
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    logger.info("📥 Received signature (" + std::to_string(blockCount) + " blocks) for " + filename, "SyncPipeline");
    
    // Extract signature data
    std::vector<uint8_t> sigData(data.begin() + offset, data.end());
    
    // Compute and send delta
    computeAndSendDelta(peerId, relativePath, sigData);
}

void SyncPipeline::computeAndSendDelta(const std::string& peerId, const std::string& relativePath,
                                       const std::vector<uint8_t>& peerSignature) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    std::string localPath = getAbsolutePath(relativePath);
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    if (!std::filesystem::exists(localPath)) {
        logger.error("Local file not found for delta: " + localPath, "SyncPipeline");
        return;
    }
    
    // Deserialize peer's signature
    auto peerSigs = DeltaSerialization::deserializeSignature(peerSignature);
    
    logger.info("🔄 Computing delta for " + filename + " against " + 
               std::to_string(peerSigs.size()) + " peer blocks", "SyncPipeline");
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Calculate delta
    auto deltas = DeltaEngine::calculateDelta(localPath, peerSigs);
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    
    // Calculate savings
    uint64_t originalSize = std::filesystem::file_size(localPath);
    uint64_t copyBytes = 0;
    uint64_t literalBytes = 0;
    
    for (const auto& delta : deltas) {
        if (!delta.isLiteral) {
            copyBytes += BLOCK_SIZE;  // COPY instruction
        } else {
            literalBytes += delta.literalData.size();  // LITERAL instruction
        }
    }
    
    double savings = originalSize > 0 ? 
        (1.0 - static_cast<double>(literalBytes) / originalSize) * 100.0 : 0.0;
    
    logger.info("📊 Delta computed in " + std::to_string(elapsed) + "ms: " +
               std::to_string(deltas.size()) + " instructions, " +
               std::to_string(static_cast<int>(savings)) + "% bandwidth saved", "SyncPipeline");
    
    metrics.recordDeltaComputeTime(elapsed);
    
    // Serialize delta
    auto serializedDelta = DeltaSerialization::serializeDelta(deltas);
    
    // Update transfer context
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            auto& ctx = activeTransfers_[it->second];
            ctx.useDelta = true;
            ctx.deltaInstructions = static_cast<uint32_t>(deltas.size());
            ctx.savedBytes = originalSize - literalBytes;
            ctx.state = TransferState::STREAMING_BLOCKS;
        }
    }
    
    // Send delta in chunks
    const size_t chunkSize = CHUNK_SIZE;
    uint32_t totalChunks = serializedDelta.empty() ? 1 :
        static_cast<uint32_t>((serializedDelta.size() + chunkSize - 1) / chunkSize);
    
    for (uint32_t i = 0; i < totalChunks; ++i) {
        size_t offset = i * chunkSize;
        size_t len = std::min(chunkSize, serializedDelta.size() - offset);
        
        // Build DELTA_RESPONSE chunk
        size_t payloadSize = sizeof(uint16_t) + sizeof(uint32_t) * 2 + relativePath.size() + len;
        std::vector<uint8_t> chunkData(sizeof(MessageHeader) + payloadSize);
        
        MessageHeader* header = reinterpret_cast<MessageHeader*>(chunkData.data());
        *header = createHeader(MessageType::DELTA_RESPONSE,
                              static_cast<uint32_t>(payloadSize),
                              getNextSequence(peerId));
        
        size_t pos = sizeof(MessageHeader);
        
        // Path length
        uint16_t pathLen = static_cast<uint16_t>(relativePath.size());
        std::memcpy(chunkData.data() + pos, &pathLen, sizeof(pathLen));
        pos += sizeof(pathLen);
        
        // Chunk info
        std::memcpy(chunkData.data() + pos, &i, sizeof(i));
        pos += sizeof(i);
        std::memcpy(chunkData.data() + pos, &totalChunks, sizeof(totalChunks));
        pos += sizeof(totalChunks);
        
        // Path
        std::memcpy(chunkData.data() + pos, relativePath.data(), relativePath.size());
        pos += relativePath.size();
        
        // Delta data
        if (len > 0) {
            std::memcpy(chunkData.data() + pos, serializedDelta.data() + offset, len);
        }
        
        if (!sendMessage(peerId, chunkData)) {
            logger.error("Failed to send delta chunk " + std::to_string(i), "SyncPipeline");
            return;
        }
        
        metrics.addBytesUploaded(chunkData.size());
    }
    
    logger.info("📤 Sent delta (" + std::to_string(serializedDelta.size()) + " bytes) for " + filename, "SyncPipeline");
    metrics.incrementDeltasSent();
}

void SyncPipeline::handleDeltaResponse(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    if (data.size() < sizeof(MessageHeader) + sizeof(uint16_t) + sizeof(uint32_t) * 2) {
        logger.error("DELTA_RESPONSE too small from " + peerId, "SyncPipeline");
        return;
    }
    
    size_t offset = sizeof(MessageHeader);
    
    uint16_t pathLen;
    std::memcpy(&pathLen, data.data() + offset, sizeof(pathLen));
    offset += sizeof(pathLen);
    
    uint32_t chunkIndex, totalChunks;
    std::memcpy(&chunkIndex, data.data() + offset, sizeof(chunkIndex));
    offset += sizeof(chunkIndex);
    std::memcpy(&totalChunks, data.data() + offset, sizeof(totalChunks));
    offset += sizeof(totalChunks);
    
    if (data.size() < offset + pathLen) {
        logger.error("DELTA_RESPONSE path truncated", "SyncPipeline");
        return;
    }
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + offset), pathLen);
    offset += pathLen;
    
    std::string filename = std::filesystem::path(relativePath).filename().string();
    std::string localPath = getAbsolutePath(relativePath);
    
    // Extract delta chunk
    std::vector<uint8_t> chunkData(data.begin() + offset, data.end());
    
    // Handle chunked transfer
    std::string key = peerId + "|" + relativePath;
    std::vector<uint8_t> fullDelta;
    
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
            pending.chunks[chunkIndex] = chunkData;
            pending.receivedChunks++;
        }
        
        if (pending.receivedChunks < pending.totalChunks) {
            logger.debug("Received delta chunk " + std::to_string(chunkIndex + 1) + "/" +
                        std::to_string(totalChunks) + " for " + filename, "SyncPipeline");
            return;
        }
        
        // All chunks received - reassemble
        for (const auto& chunk : pending.chunks) {
            fullDelta.insert(fullDelta.end(), chunk.begin(), chunk.end());
        }
        
        pendingChunks_.erase(key);
    }
    
    logger.info("📥 Received complete delta (" + std::to_string(fullDelta.size()) + " bytes) for " + filename, "SyncPipeline");
    
    // Deserialize and apply delta
    auto deltas = DeltaSerialization::deserializeDelta(fullDelta);
    
    logger.debug("Applying " + std::to_string(deltas.size()) + " delta instructions", "SyncPipeline");
    
    // Create parent directories if needed
    std::filesystem::path parentDir = std::filesystem::path(localPath).parent_path();
    if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
        std::filesystem::create_directories(parentDir);
    }
    
    // Create empty file if doesn't exist
    if (!std::filesystem::exists(localPath)) {
        std::ofstream outfile(localPath);
        outfile.close();
    }
    
    // Apply delta
    auto newData = DeltaEngine::applyDelta(localPath, deltas);
    
    // Mark as patched BEFORE writing to prevent sync loop
    if (markAsPatchedCallback_) {
        markAsPatchedCallback_(filename);
    }
    
    // Write updated file
    if (filesystem_) {
        filesystem_->writeFile(localPath, newData);
    }
    
    // Update transfer and send completion
    std::string transferId;
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            transferId = it->second;
            auto& ctx = activeTransfers_[transferId];
            ctx.bytesTransferred = ctx.fileSize;
            ctx.state = TransferState::AWAITING_ACK;
        }
    }
    
    if (!transferId.empty()) {
        sendTransferComplete(peerId, transferId);
    }
    
    logger.info("✅ Successfully applied delta to " + filename, "SyncPipeline");
    metrics.incrementDeltasReceived();
    metrics.incrementFilesSynced();
    metrics.addBytesDownloaded(fullDelta.size());
}

} // namespace Sync
} // namespace SentinelFS
