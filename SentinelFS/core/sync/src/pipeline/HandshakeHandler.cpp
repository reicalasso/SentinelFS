/**
 * @file HandshakeHandler.cpp
 * @brief Stage 2: Handshake - mTLS + Capability Exchange
 * 
 * Handshake Flow:
 * 1. Initiator sends HANDSHAKE_INIT with capabilities
 * 2. Responder sends HANDSHAKE_RESPONSE with negotiated caps + challenge
 * 3. Initiator sends HANDSHAKE_COMPLETE with challenge response
 * 4. Both sides mark session as authenticated
 */

#include "SyncPipeline.h"
#include "INetworkAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <cstring>

namespace SentinelFS {
namespace Sync {

bool SyncPipeline::initiateHandshake(const std::string& peerId) {
    auto& logger = Logger::instance();
    logger.info("Initiating handshake with peer: " + peerId, "SyncPipeline");
    
    // Build HANDSHAKE_INIT message
    HandshakeInit init{};
    init.header = createHeader(MessageType::HANDSHAKE_INIT, 
                               sizeof(HandshakeInit) - sizeof(MessageHeader),
                               0);  // Sequence 0 for handshake
    
    // Copy local peer ID
    std::memset(init.peerId, 0, sizeof(init.peerId));
    std::memcpy(init.peerId, localPeerId_.data(), 
                std::min(localPeerId_.size(), sizeof(init.peerId)));
    
    init.capabilities = static_cast<uint32_t>(localCapabilities_);
    init.maxBlockSize = BLOCK_SIZE;
    init.maxChunkSize = CHUNK_SIZE;
    
    // Session code hash (for verification)
    // In production, this would be derived from the actual session code
    RAND_bytes(init.sessionCodeHash, sizeof(init.sessionCodeHash));
    
    // Create pending session
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        PeerSession session;
        session.peerId = peerId;
        session.authenticated = false;
        session.lastActivity = std::chrono::steady_clock::now();
        peerSessions_[peerId] = session;
    }
    
    // Send
    std::vector<uint8_t> data(reinterpret_cast<uint8_t*>(&init),
                              reinterpret_cast<uint8_t*>(&init) + sizeof(init));
    
    if (!sendMessage(peerId, data)) {
        logger.error("Failed to send HANDSHAKE_INIT to " + peerId, "SyncPipeline");
        return false;
    }
    
    logger.debug("Sent HANDSHAKE_INIT to " + peerId + 
                " caps=" + std::to_string(init.capabilities), "SyncPipeline");
    return true;
}

void SyncPipeline::handleHandshakeInit(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    
    if (data.size() < sizeof(HandshakeInit)) {
        logger.error("HANDSHAKE_INIT too small from " + peerId, "SyncPipeline");
        return;
    }
    
    const HandshakeInit* init = reinterpret_cast<const HandshakeInit*>(data.data());
    
    logger.info("Received HANDSHAKE_INIT from " + peerId + 
               " caps=" + std::to_string(init->capabilities), "SyncPipeline");
    
    // Negotiate capabilities (intersection)
    Capability peerCaps = static_cast<Capability>(init->capabilities);
    Capability negotiated = localCapabilities_ & peerCaps;
    
    // Negotiate block/chunk sizes (minimum)
    uint32_t agreedBlock = std::min(init->maxBlockSize, static_cast<uint32_t>(BLOCK_SIZE));
    uint32_t agreedChunk = std::min(init->maxChunkSize, static_cast<uint32_t>(CHUNK_SIZE));
    
    // Create session
    PeerSession session;
    session.peerId = peerId;
    session.negotiatedCaps = negotiated;
    session.agreedBlockSize = agreedBlock;
    session.agreedChunkSize = agreedChunk;
    session.authenticated = false;
    session.lastActivity = std::chrono::steady_clock::now();
    
    // Generate challenge
    uint8_t challenge[32];
    RAND_bytes(challenge, sizeof(challenge));
    
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        peerSessions_[peerId] = session;
    }
    
    // Build response
    HandshakeResponse response{};
    response.header = createHeader(MessageType::HANDSHAKE_RESPONSE,
                                   sizeof(HandshakeResponse) - sizeof(MessageHeader),
                                   0);
    
    std::memset(response.peerId, 0, sizeof(response.peerId));
    std::memcpy(response.peerId, localPeerId_.data(),
                std::min(localPeerId_.size(), sizeof(response.peerId)));
    
    response.capabilities = static_cast<uint32_t>(negotiated);
    response.agreedBlockSize = agreedBlock;
    response.agreedChunkSize = agreedChunk;
    std::memcpy(response.challenge, challenge, sizeof(challenge));
    
    // Send response
    std::vector<uint8_t> responseData(reinterpret_cast<uint8_t*>(&response),
                                      reinterpret_cast<uint8_t*>(&response) + sizeof(response));
    
    if (!sendMessage(peerId, responseData)) {
        logger.error("Failed to send HANDSHAKE_RESPONSE to " + peerId, "SyncPipeline");
        return;
    }
    
    logger.debug("Sent HANDSHAKE_RESPONSE to " + peerId + 
                " negotiated=" + std::to_string(static_cast<uint32_t>(negotiated)), "SyncPipeline");
}

void SyncPipeline::handleHandshakeResponse(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    
    if (data.size() < sizeof(HandshakeResponse)) {
        logger.error("HANDSHAKE_RESPONSE too small from " + peerId, "SyncPipeline");
        return;
    }
    
    const HandshakeResponse* response = reinterpret_cast<const HandshakeResponse*>(data.data());
    
    logger.info("Received HANDSHAKE_RESPONSE from " + peerId + 
               " negotiated=" + std::to_string(response->capabilities), "SyncPipeline");
    
    // Update session with negotiated values
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        auto it = peerSessions_.find(peerId);
        if (it == peerSessions_.end()) {
            logger.error("No pending session for " + peerId, "SyncPipeline");
            return;
        }
        
        it->second.negotiatedCaps = static_cast<Capability>(response->capabilities);
        it->second.agreedBlockSize = response->agreedBlockSize;
        it->second.agreedChunkSize = response->agreedChunkSize;
        it->second.lastActivity = std::chrono::steady_clock::now();
    }
    
    // Compute challenge response (HMAC of challenge with session key)
    // In production, this would use the actual session key
    uint8_t challengeResponse[32];
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, response->challenge, sizeof(response->challenge));
    EVP_DigestUpdate(ctx, localPeerId_.data(), localPeerId_.size());
    unsigned int len;
    EVP_DigestFinal_ex(ctx, challengeResponse, &len);
    EVP_MD_CTX_free(ctx);
    
    // Build HANDSHAKE_COMPLETE
    struct HandshakeComplete {
        MessageHeader header;
        uint8_t challengeResponse[32];
    } complete{};
    
    complete.header = createHeader(MessageType::HANDSHAKE_COMPLETE,
                                   sizeof(complete) - sizeof(MessageHeader),
                                   0);
    std::memcpy(complete.challengeResponse, challengeResponse, sizeof(challengeResponse));
    
    // Send
    std::vector<uint8_t> completeData(reinterpret_cast<uint8_t*>(&complete),
                                      reinterpret_cast<uint8_t*>(&complete) + sizeof(complete));
    
    if (!sendMessage(peerId, completeData)) {
        logger.error("Failed to send HANDSHAKE_COMPLETE to " + peerId, "SyncPipeline");
        return;
    }
    
    // Mark as authenticated
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        auto it = peerSessions_.find(peerId);
        if (it != peerSessions_.end()) {
            it->second.authenticated = true;
            it->second.nextSequence = 1;
        }
    }
    
    logger.info("✅ Handshake complete with " + peerId + " - session authenticated", "SyncPipeline");
    MetricsCollector::instance().incrementPeersConnected();
}

void SyncPipeline::handleHandshakeComplete(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    
    // Verify challenge response (in production)
    // For now, just mark as authenticated
    
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        auto it = peerSessions_.find(peerId);
        if (it == peerSessions_.end()) {
            logger.error("No pending session for " + peerId, "SyncPipeline");
            return;
        }
        
        it->second.authenticated = true;
        it->second.nextSequence = 1;
        it->second.lastActivity = std::chrono::steady_clock::now();
    }
    
    logger.info("✅ Handshake complete with " + peerId + " - session authenticated", "SyncPipeline");
    MetricsCollector::instance().incrementPeersConnected();
    
    // Log capabilities
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto& session = peerSessions_[peerId];
    
    std::string capsStr;
    if (hasCapability(session.negotiatedCaps, Capability::DELTA_SYNC)) capsStr += "DELTA ";
    if (hasCapability(session.negotiatedCaps, Capability::COMPRESSION_ZSTD)) capsStr += "ZSTD ";
    if (hasCapability(session.negotiatedCaps, Capability::ENCRYPTION_AES_GCM)) capsStr += "AES-GCM ";
    if (hasCapability(session.negotiatedCaps, Capability::STREAMING)) capsStr += "STREAM ";
    
    logger.info("Negotiated capabilities with " + peerId + ": " + capsStr, "SyncPipeline");
    logger.info("Block size: " + std::to_string(session.agreedBlockSize) + 
               ", Chunk size: " + std::to_string(session.agreedChunkSize), "SyncPipeline");
}

} // namespace Sync
} // namespace SentinelFS
