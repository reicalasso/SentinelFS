/**
 * @file FinalizeHandler.cpp
 * @brief Stage 7: ACK/Finalize - Integrity verification and completion
 * 
 * Finalize Flow:
 * 1. Sender sends TRANSFER_COMPLETE with final hash
 * 2. Receiver verifies hash matches computed hash
 * 3. Receiver sends TRANSFER_ACK with verification result
 * 4. If hash mismatch, receiver sends INTEGRITY_FAIL
 * 5. On failure, sender can retry the transfer
 */

#include "SyncPipeline.h"
#include "INetworkAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <cstring>
#include <chrono>

namespace SentinelFS {
namespace Sync {

void SyncPipeline::sendTransferComplete(const std::string& peerId, const std::string& transferId) {
    auto& logger = Logger::instance();
    
    TransferContext ctx;
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = activeTransfers_.find(transferId);
        if (it == activeTransfers_.end()) {
            logger.error("Transfer not found: " + transferId, "SyncPipeline");
            return;
        }
        ctx = it->second;
    }
    
    std::string filename = std::filesystem::path(ctx.relativePath).filename().string();
    
    // Calculate duration
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.startTime).count();
    
    // Build TRANSFER_COMPLETE message
    size_t payloadSize = sizeof(TransferComplete) - sizeof(MessageHeader) + ctx.relativePath.size();
    std::vector<uint8_t> msgData(sizeof(TransferComplete) + ctx.relativePath.size());
    
    TransferComplete* complete = reinterpret_cast<TransferComplete*>(msgData.data());
    complete->header = createHeader(MessageType::TRANSFER_COMPLETE,
                                    static_cast<uint32_t>(payloadSize),
                                    getNextSequence(peerId));
    complete->pathLength = static_cast<uint16_t>(ctx.relativePath.size());
    
    // Copy file hash
    if (ctx.fileHash.size() >= 32) {
        std::memcpy(complete->finalHash, ctx.fileHash.data(), 32);
    } else {
        // Recalculate hash
        auto hash = calculateFileHash(ctx.localPath);
        std::memcpy(complete->finalHash, hash.data(), std::min(hash.size(), sizeof(complete->finalHash)));
    }
    
    complete->bytesTransferred = ctx.bytesTransferred;
    complete->durationMs = static_cast<uint32_t>(duration);
    
    // Append path
    std::memcpy(msgData.data() + sizeof(TransferComplete), ctx.relativePath.data(), ctx.relativePath.size());
    
    if (!sendMessage(peerId, msgData)) {
        logger.error("Failed to send TRANSFER_COMPLETE for " + filename, "SyncPipeline");
        return;
    }
    
    updateTransferState(transferId, TransferState::AWAITING_ACK);
    
    // Calculate transfer rate
    double rate = duration > 0 ? (ctx.bytesTransferred * 1000.0 / duration / 1024.0) : 0.0;
    
    logger.info("📋 Sent TRANSFER_COMPLETE for " + filename + 
               " (" + std::to_string(ctx.bytesTransferred) + " bytes in " +
               std::to_string(duration) + "ms, " + 
               std::to_string(static_cast<int>(rate)) + " KB/s)", "SyncPipeline");
}

void SyncPipeline::handleTransferComplete(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    if (data.size() < sizeof(TransferComplete)) {
        logger.error("TRANSFER_COMPLETE too small from " + peerId, "SyncPipeline");
        return;
    }
    
    const TransferComplete* complete = reinterpret_cast<const TransferComplete*>(data.data());
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + sizeof(TransferComplete)),
                             complete->pathLength);
    std::string localPath = getAbsolutePath(relativePath);
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    logger.info("📋 Received TRANSFER_COMPLETE for " + filename + 
               " (" + std::to_string(complete->bytesTransferred) + " bytes)", "SyncPipeline");
    
    // Verify integrity
    bool verified = false;
    std::vector<uint8_t> computedHash;
    
    if (std::filesystem::exists(localPath)) {
        computedHash = calculateFileHash(localPath);
        
        if (computedHash.size() >= 32) {
            verified = (std::memcmp(computedHash.data(), complete->finalHash, 32) == 0);
        }
    }
    
    // Build response
    size_t payloadSize = sizeof(TransferAck) - sizeof(MessageHeader) + relativePath.size();
    std::vector<uint8_t> ackData(sizeof(TransferAck) + relativePath.size());
    
    TransferAck* ack = reinterpret_cast<TransferAck*>(ackData.data());
    ack->header = createHeader(verified ? MessageType::TRANSFER_ACK : MessageType::INTEGRITY_FAIL,
                               static_cast<uint32_t>(payloadSize),
                               getNextSequence(peerId));
    ack->pathLength = static_cast<uint16_t>(relativePath.size());
    ack->verified = verified ? 1 : 0;
    
    if (!computedHash.empty()) {
        std::memcpy(ack->computedHash, computedHash.data(), 
                   std::min(computedHash.size(), sizeof(ack->computedHash)));
    }
    
    std::memcpy(ackData.data() + sizeof(TransferAck), relativePath.data(), relativePath.size());
    
    if (!sendMessage(peerId, ackData)) {
        logger.error("Failed to send TRANSFER_ACK for " + filename, "SyncPipeline");
        return;
    }
    
    // Update transfer state
    std::string transferId;
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            transferId = it->second;
        }
    }
    
    if (verified) {
        logger.info("✅ Integrity verified for " + filename + " - transfer complete", "SyncPipeline");
        
        if (!transferId.empty()) {
            updateTransferState(transferId, TransferState::COMPLETE);
            
            if (completeCallback_) {
                completeCallback_(transferId, true, "");
            }
            
            // Cleanup
            std::lock_guard<std::mutex> lock(transferMutex_);
            pathToTransfer_.erase(relativePath + "|" + peerId);
            activeTransfers_.erase(transferId);
        }
        
        metrics.incrementTransfersCompleted();
        metrics.recordSyncLatency(complete->durationMs);
    } else {
        logger.error("❌ Integrity check FAILED for " + filename, "SyncPipeline");
        
        // Log hash mismatch for debugging
        std::string expectedHash, computedHashStr;
        for (int i = 0; i < 8; ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", complete->finalHash[i]);
            expectedHash += buf;
        }
        if (!computedHash.empty()) {
            for (int i = 0; i < 8; ++i) {
                char buf[3];
                snprintf(buf, sizeof(buf), "%02x", computedHash[i]);
                computedHashStr += buf;
            }
        }
        logger.error("Expected: " + expectedHash + "..., Got: " + computedHashStr + "...", "SyncPipeline");
        
        if (!transferId.empty()) {
            updateTransferState(transferId, TransferState::FAILED);
            
            if (completeCallback_) {
                completeCallback_(transferId, false, "Integrity check failed");
            }
        }
        
        metrics.incrementTransfersFailed();
        metrics.incrementSyncErrors();
    }
}

void SyncPipeline::handleTransferAck(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    if (data.size() < sizeof(TransferAck)) {
        logger.error("TRANSFER_ACK too small from " + peerId, "SyncPipeline");
        return;
    }
    
    const TransferAck* ack = reinterpret_cast<const TransferAck*>(data.data());
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + sizeof(TransferAck)),
                             ack->pathLength);
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    // Find transfer
    std::string transferId;
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            transferId = it->second;
        }
    }
    
    if (ack->verified) {
        logger.info("✅ Transfer of " + filename + " to " + peerId + " confirmed", "SyncPipeline");
        
        if (!transferId.empty()) {
            updateTransferState(transferId, TransferState::COMPLETE);
            
            if (completeCallback_) {
                completeCallback_(transferId, true, "");
            }
            
            // Cleanup
            std::lock_guard<std::mutex> lock(transferMutex_);
            pathToTransfer_.erase(relativePath + "|" + peerId);
            activeTransfers_.erase(transferId);
        }
        
        metrics.incrementTransfersCompleted();
    } else {
        logger.error("❌ Transfer of " + filename + " to " + peerId + " failed verification", "SyncPipeline");
        
        // Could implement retry here
        if (!transferId.empty()) {
            std::lock_guard<std::mutex> lock(transferMutex_);
            auto it = activeTransfers_.find(transferId);
            if (it != activeTransfers_.end() && it->second.retryCount < MAX_RETRIES) {
                it->second.retryCount++;
                logger.info("Retrying transfer (" + std::to_string(it->second.retryCount) + 
                           "/" + std::to_string(MAX_RETRIES) + ")", "SyncPipeline");
                // Re-initiate transfer
                // syncFileToPeer(peerId, it->second.localPath);
            } else {
                updateTransferState(transferId, TransferState::FAILED);
                if (completeCallback_) {
                    completeCallback_(transferId, false, "Max retries exceeded");
                }
            }
        }
        
        metrics.incrementTransfersFailed();
    }
}

void SyncPipeline::handleIntegrityFail(const std::string& peerId, const std::vector<uint8_t>& data) {
    // Same as handleTransferAck with verified=false
    auto& logger = Logger::instance();
    
    if (data.size() < sizeof(TransferAck)) {
        return;
    }
    
    const TransferAck* ack = reinterpret_cast<const TransferAck*>(data.data());
    
    std::string relativePath(reinterpret_cast<const char*>(data.data() + sizeof(TransferAck)),
                             ack->pathLength);
    std::string filename = std::filesystem::path(relativePath).filename().string();
    
    logger.error("🚨 INTEGRITY_FAIL received for " + filename + " from " + peerId, "SyncPipeline");
    
    // Find and handle transfer
    std::string transferId;
    {
        std::lock_guard<std::mutex> lock(transferMutex_);
        auto it = pathToTransfer_.find(relativePath + "|" + peerId);
        if (it != pathToTransfer_.end()) {
            transferId = it->second;
            auto& ctx = activeTransfers_[transferId];
            
            if (ctx.retryCount < MAX_RETRIES) {
                ctx.retryCount++;
                ctx.state = TransferState::SENDING_META;
                logger.info("Retrying transfer (" + std::to_string(ctx.retryCount) + 
                           "/" + std::to_string(MAX_RETRIES) + ")", "SyncPipeline");
                // Will retry on next sync cycle
            } else {
                ctx.state = TransferState::FAILED;
                ctx.lastError = "Integrity check failed after " + std::to_string(MAX_RETRIES) + " retries";
                
                if (completeCallback_) {
                    completeCallback_(transferId, false, ctx.lastError);
                }
            }
        }
    }
    
    MetricsCollector::instance().incrementSyncErrors();
}

} // namespace Sync
} // namespace SentinelFS
