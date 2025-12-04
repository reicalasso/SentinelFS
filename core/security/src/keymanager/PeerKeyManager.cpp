/**
 * @file PeerKeyManager.cpp
 * @brief Peer public key management
 */

#include "KeyManager.h"
#include "Logger.h"

namespace SentinelFS {

bool KeyManager::addPeerKey(const std::string& peerId,
                            const std::vector<uint8_t>& publicKey,
                            bool verified) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    peerPublicKeys_[peerId] = publicKey;
    peerKeyVerified_[peerId] = verified;
    
    // Store in key store
    KeyInfo info;
    info.keyId = peerId + "_public";
    info.type = KeyType::IDENTITY_PUBLIC;
    info.algorithm = "Ed25519";
    info.peerId = peerId;
    info.created = std::chrono::system_clock::now();
    
    keyStore_->store(info.keyId, publicKey, info);
    
    return true;
}

void KeyManager::removePeerKeys(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    peerPublicKeys_.erase(peerId);
    peerKeyVerified_.erase(peerId);
    sessionKeys_.erase(peerId);
    
    keyStore_->remove(peerId + "_public");
}

std::vector<uint8_t> KeyManager::getPeerPublicKey(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = peerPublicKeys_.find(peerId);
    if (it != peerPublicKeys_.end()) {
        return it->second;
    }
    
    return {};
}

bool KeyManager::isPeerKeyVerified(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = peerKeyVerified_.find(peerId);
    return it != peerKeyVerified_.end() && it->second;
}

void KeyManager::markPeerKeyVerified(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    peerKeyVerified_[peerId] = true;
}

bool KeyManager::upgradeToIdentityAuth(const std::string& peerId,
                                        const std::vector<uint8_t>& peerPublicKey) {
    auto& logger = Logger::instance();
    
    // Verify the public key
    if (peerPublicKey.size() != 32) {
        logger.log(LogLevel::WARN, "Invalid peer public key size", "KeyManager");
        return false;
    }
    
    // Add peer key (initially unverified)
    if (!addPeerKey(peerId, peerPublicKey, false)) {
        return false;
    }
    
    logger.log(LogLevel::INFO, "Upgraded to identity auth for peer: " + peerId, "KeyManager");
    logger.log(LogLevel::INFO, "Key fingerprint should be verified out-of-band", "KeyManager");
    
    return true;
}

} // namespace SentinelFS
