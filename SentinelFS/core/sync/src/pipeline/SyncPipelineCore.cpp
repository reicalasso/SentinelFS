/**
 * @file SyncPipelineCore.cpp
 * @brief Core SyncPipeline implementation - construction, helpers, message routing
 */

#include "SyncPipeline.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace SentinelFS {
namespace Sync {

SyncPipeline::SyncPipeline(INetworkAPI* network, IStorageAPI* storage,
                           IFileAPI* filesystem, const std::string& watchDir)
    : network_(network)
    , storage_(storage)
    , filesystem_(filesystem)
    , watchDirectory_(watchDir)
{
    auto& logger = Logger::instance();
    
    // Generate local peer ID if not available from network
    if (network_) {
        // Try to get from network layer
        // For now, generate a random one
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        for (int i = 0; i < 32; ++i) {
            ss << std::hex << dis(gen);
        }
        localPeerId_ = ss.str();
    }
    
    logger.info("SyncPipeline initialized for: " + watchDir, "SyncPipeline");
    logger.debug("Local capabilities: " + std::to_string(static_cast<uint32_t>(localCapabilities_)), "SyncPipeline");
}

SyncPipeline::~SyncPipeline() {
    running_ = false;
}

// ============================================================================
// Helper Methods
// ============================================================================

std::string SyncPipeline::generateTransferId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "xfer-";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string SyncPipeline::getRelativePath(const std::string& absolutePath) const {
    if (absolutePath.find(watchDirectory_) == 0) {
        std::string rel = absolutePath.substr(watchDirectory_.length());
        if (!rel.empty() && rel[0] == '/') {
            rel = rel.substr(1);
        }
        return rel;
    }
    return std::filesystem::path(absolutePath).filename().string();
}

std::string SyncPipeline::getAbsolutePath(const std::string& relativePath) const {
    std::string path = watchDirectory_;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    path += relativePath;
    return path;
}

std::vector<uint8_t> SyncPipeline::calculateFileHash(const std::string& path) const {
    std::vector<uint8_t> hash(32, 0);
    
    std::ifstream file(path, std::ios::binary);
    if (!file) return hash;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return hash;
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return hash;
    }
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        EVP_DigestUpdate(ctx, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        EVP_DigestUpdate(ctx, buffer, file.gcount());
    }
    
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(ctx, hash.data(), &hashLen);
    EVP_MD_CTX_free(ctx);
    
    return hash;
}

void SyncPipeline::updateTransferState(const std::string& transferId, TransferState newState) {
    std::lock_guard<std::mutex> lock(transferMutex_);
    
    auto it = activeTransfers_.find(transferId);
    if (it == activeTransfers_.end()) return;
    
    TransferState oldState = it->second.state;
    it->second.state = newState;
    it->second.lastActivity = std::chrono::steady_clock::now();
    
    auto& logger = Logger::instance();
    logger.debug("Transfer " + transferId + " state: " + 
                 transferStateName(oldState) + " -> " + transferStateName(newState), 
                 "SyncPipeline");
    
    if (stateChangeCallback_) {
        stateChangeCallback_(it->second.peerId, oldState, newState);
    }
}

uint32_t SyncPipeline::getNextSequence(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = peerSessions_.find(peerId);
    if (it != peerSessions_.end()) {
        return it->second.nextSequence++;
    }
    return 0;
}

bool SyncPipeline::validateSequence(const std::string& peerId, uint32_t sequence) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = peerSessions_.find(peerId);
    if (it == peerSessions_.end()) return false;
    
    auto& session = it->second;
    
    // Check for replay
    for (uint32_t seen : session.receivedSequences) {
        if (seen == sequence) {
            Logger::instance().warn("Replay detected from " + peerId + 
                                   " seq=" + std::to_string(sequence), "SyncPipeline");
            return false;
        }
    }
    
    // Add to history
    session.receivedSequences.push_back(sequence);
    if (session.receivedSequences.size() > PeerSession::MAX_SEQUENCE_HISTORY) {
        session.receivedSequences.erase(session.receivedSequences.begin());
    }
    
    return true;
}

bool SyncPipeline::sendMessage(const std::string& peerId, const std::vector<uint8_t>& data) {
    if (!network_) return false;
    return network_->sendData(peerId, data);
}

void SyncPipeline::cleanupStaleTransfers() {
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(transferMutex_);
    
    for (auto it = activeTransfers_.begin(); it != activeTransfers_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastActivity).count();
        
        if (elapsed > TRANSFER_TIMEOUT_SECONDS) {
            Logger::instance().warn("Transfer " + it->first + " timed out", "SyncPipeline");
            
            if (completeCallback_) {
                completeCallback_(it->first, false, "Transfer timed out");
            }
            
            pathToTransfer_.erase(it->second.relativePath);
            it = activeTransfers_.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Message Routing
// ============================================================================

void SyncPipeline::handleMessage(const std::string& peerId, const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(MessageHeader)) {
        // Try legacy text-based protocol for backward compatibility
        std::string msg(data.begin(), data.end());
        
        if (msg.find("UPDATE_AVAILABLE|") == 0 || 
            msg.find("REQUEST_DELTA|") == 0 ||
            msg.find("DELTA_DATA|") == 0) {
            // Legacy protocol - let DeltaSyncProtocolHandler handle it
            return;
        }
        
        Logger::instance().warn("Message too small from " + peerId, "SyncPipeline");
        return;
    }
    
    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data.data());
    
    // Validate header
    if (!validateHeader(*header)) {
        Logger::instance().warn("Invalid message header from " + peerId, "SyncPipeline");
        return;
    }
    
    MessageType type = static_cast<MessageType>(header->type);
    
    auto& logger = Logger::instance();
    logger.debug("Received " + std::string(messageTypeName(type)) + " from " + peerId, "SyncPipeline");
    
    // Route to appropriate handler
    switch (type) {
        // Stage 2: Handshake
        case MessageType::HANDSHAKE_INIT:
            handleHandshakeInit(peerId, data);
            break;
        case MessageType::HANDSHAKE_RESPONSE:
            handleHandshakeResponse(peerId, data);
            break;
        case MessageType::HANDSHAKE_COMPLETE:
            handleHandshakeComplete(peerId, data);
            break;
            
        // Stage 3: Meta Transfer
        case MessageType::FILE_META:
            handleFileMeta(peerId, data);
            break;
        case MessageType::FILE_META_ACK:
            handleFileMetaAck(peerId, data);
            break;
            
        // Stage 4: Hash Scan
        case MessageType::SIGNATURE_REQUEST:
            handleSignatureRequest(peerId, data);
            break;
        case MessageType::SIGNATURE_RESPONSE:
            handleSignatureResponse(peerId, data);
            break;
            
        // Stage 5: Delta
        case MessageType::DELTA_RESPONSE:
            handleDeltaResponse(peerId, data);
            break;
            
        // Stage 6: Block Stream
        case MessageType::BLOCK_DATA:
            handleBlockData(peerId, data);
            break;
        case MessageType::BLOCK_ACK:
            handleBlockAck(peerId, data);
            break;
            
        // Stage 7: Finalize
        case MessageType::TRANSFER_COMPLETE:
            handleTransferComplete(peerId, data);
            break;
        case MessageType::TRANSFER_ACK:
            handleTransferAck(peerId, data);
            break;
        case MessageType::INTEGRITY_FAIL:
            handleIntegrityFail(peerId, data);
            break;
            
        // Control
        case MessageType::PING:
            // Send PONG
            {
                MessageHeader pong = createHeader(MessageType::PONG, 0, getNextSequence(peerId));
                std::vector<uint8_t> pongData(reinterpret_cast<uint8_t*>(&pong),
                                              reinterpret_cast<uint8_t*>(&pong) + sizeof(pong));
                sendMessage(peerId, pongData);
            }
            break;
            
        default:
            logger.warn("Unknown message type " + std::to_string(header->type) + 
                       " from " + peerId, "SyncPipeline");
            break;
    }
}

// ============================================================================
// High-Level API
// ============================================================================

std::string SyncPipeline::syncFileToPeer(const std::string& peerId, const std::string& localPath) {
    auto& logger = Logger::instance();
    
    // Check if peer is authenticated
    if (!isPeerAuthenticated(peerId)) {
        logger.info("Peer " + peerId + " not authenticated, initiating handshake", "SyncPipeline");
        if (!initiateHandshake(peerId)) {
            logger.error("Failed to initiate handshake with " + peerId, "SyncPipeline");
            return "";
        }
        // Handshake is async, caller should retry after PEER_AUTHENTICATED event
        return "";
    }
    
    // Start with meta transfer
    return sendFileMeta(peerId, localPath);
}

void SyncPipeline::broadcastFileUpdate(const std::string& localPath) {
    auto& logger = Logger::instance();
    
    if (!storage_) {
        logger.error("Storage not available for broadcast", "SyncPipeline");
        return;
    }
    
    auto peers = storage_->getAllPeers();
    if (peers.empty()) {
        logger.debug("No peers to broadcast to", "SyncPipeline");
        return;
    }
    
    std::string filename = std::filesystem::path(localPath).filename().string();
    logger.info("Broadcasting " + filename + " to " + std::to_string(peers.size()) + " peer(s)", "SyncPipeline");
    
    for (const auto& peer : peers) {
        syncFileToPeer(peer.id, localPath);
    }
}

const TransferContext* SyncPipeline::getTransfer(const std::string& transferId) const {
    std::lock_guard<std::mutex> lock(transferMutex_);
    auto it = activeTransfers_.find(transferId);
    if (it != activeTransfers_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<TransferContext> SyncPipeline::getActiveTransfers() const {
    std::lock_guard<std::mutex> lock(transferMutex_);
    std::vector<TransferContext> result;
    for (const auto& [id, ctx] : activeTransfers_) {
        result.push_back(ctx);
    }
    return result;
}

void SyncPipeline::abortTransfer(const std::string& transferId) {
    auto& logger = Logger::instance();
    
    std::lock_guard<std::mutex> lock(transferMutex_);
    auto it = activeTransfers_.find(transferId);
    if (it == activeTransfers_.end()) return;
    
    // Send abort message
    MessageHeader abort = createHeader(MessageType::TRANSFER_ABORT, 0, 
                                       getNextSequence(it->second.peerId));
    std::vector<uint8_t> abortData(reinterpret_cast<uint8_t*>(&abort),
                                   reinterpret_cast<uint8_t*>(&abort) + sizeof(abort));
    sendMessage(it->second.peerId, abortData);
    
    logger.info("Aborted transfer " + transferId, "SyncPipeline");
    
    if (completeCallback_) {
        completeCallback_(transferId, false, "Transfer aborted by user");
    }
    
    pathToTransfer_.erase(it->second.relativePath);
    activeTransfers_.erase(it);
}

bool SyncPipeline::isPeerAuthenticated(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = peerSessions_.find(peerId);
    return it != peerSessions_.end() && it->second.authenticated;
}

} // namespace Sync
} // namespace SentinelFS
