/**
 * @file SessionKeyManager.cpp
 * @brief Session key derivation, rotation, and management
 */

#include "KeyManager.h"
#include "Crypto.h"
#include "Logger.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/kdf.h>

namespace SentinelFS {

std::string KeyManager::deriveSessionKey(
    const std::string& peerId,
    const std::vector<uint8_t>& peerPublicKey,
    bool isInitiator,
    std::chrono::seconds sessionDuration) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto& logger = Logger::instance();
    
    // Generate ephemeral X25519 keypair
    auto [ephemeralPub, ephemeralPriv] = generateEphemeralKeyPair();
    
    // Perform ECDH
    auto sharedSecret = performECDH(ephemeralPriv, peerPublicKey);
    
    if (sharedSecret.empty()) {
        logger.log(LogLevel::ERROR, "ECDH failed", "KeyManager");
        return "";
    }
    
    // Derive session key from shared secret
    std::string context = "SentinelFS-Session-" + peerId;
    auto sessionKey = deriveSessionKeyFromShared(sharedSecret, context, isInitiator);
    
    // Clear sensitive data
    OPENSSL_cleanse(ephemeralPriv.data(), ephemeralPriv.size());
    OPENSSL_cleanse(sharedSecret.data(), sharedSecret.size());
    
    // Create session key entry
    SessionKey session;
    session.key = sessionKey;
    session.peerId = peerId;
    session.created = std::chrono::system_clock::now();
    session.expires = session.created + sessionDuration;
    session.lastUsed = session.created;
    
    // Generate key ID
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(sessionKey.data(), sessionKey.size(), hash);
    session.keyId = Crypto::toHex(std::vector<uint8_t>(hash, hash + 8));
    
    // Store keyId before moving session (avoid accessing moved-from object)
    std::string keyId = session.keyId;
    sessionKeys_[peerId] = std::move(session);
    
    logger.log(LogLevel::INFO, "Derived session key for peer: " + peerId, "KeyManager");
    
    return keyId;
}

std::vector<uint8_t> KeyManager::getSessionKey(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessionKeys_.find(peerId);
    if (it == sessionKeys_.end()) {
        return {};
    }
    
    if (it->second.needsRotation()) {
        return {};  // Force re-negotiation
    }
    
    it->second.lastUsed = std::chrono::system_clock::now();
    return it->second.key;
}

bool KeyManager::sessionNeedsRotation(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessionKeys_.find(peerId);
    if (it == sessionKeys_.end()) {
        return true;
    }
    
    return it->second.needsRotation();
}

void KeyManager::recordKeyUsage(const std::string& peerId, uint64_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessionKeys_.find(peerId);
    if (it != sessionKeys_.end()) {
        it->second.bytesEncrypted += bytes;
        it->second.messagesEncrypted++;
        
        // Check for rotation
        if (it->second.needsRotation() && rotationCallback_) {
            rotationCallback_(peerId);
        }
    }
}

void KeyManager::invalidateSession(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessionKeys_.find(peerId);
    if (it != sessionKeys_.end()) {
        OPENSSL_cleanse(it->second.key.data(), it->second.key.size());
        sessionKeys_.erase(it);
    }
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> 
KeyManager::generateEphemeralKeyPair() {
    // Generate X25519 keypair
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
    if (!ctx) {
        return {{}, {}};
    }
    
    if (EVP_PKEY_keygen_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        return {{}, {}};
    }
    
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        return {{}, {}};
    }
    EVP_PKEY_CTX_free(ctx);
    
    std::vector<uint8_t> publicKey(32);
    std::vector<uint8_t> privateKey(32);
    
    size_t len = 32;
    if (EVP_PKEY_get_raw_public_key(pkey, publicKey.data(), &len) != 1 ||
        EVP_PKEY_get_raw_private_key(pkey, privateKey.data(), &len) != 1) {
        EVP_PKEY_free(pkey);
        return {{}, {}};
    }
    
    EVP_PKEY_free(pkey);
    
    return {publicKey, privateKey};
}

size_t KeyManager::cleanupExpiredKeys() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t removed = 0;
    
    for (auto it = sessionKeys_.begin(); it != sessionKeys_.end(); ) {
        if (it->second.needsRotation()) {
            OPENSSL_cleanse(it->second.key.data(), it->second.key.size());
            it = sessionKeys_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    return removed;
}

void KeyManager::setKeyRotationCallback(KeyRotationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    rotationCallback_ = std::move(callback);
}

std::vector<uint8_t> KeyManager::performECDH(
    const std::vector<uint8_t>& privateKey,
    const std::vector<uint8_t>& peerPublicKey) {
    
    // Create local private key
    EVP_PKEY* privKey = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_X25519, nullptr, privateKey.data(), privateKey.size());
    
    // Create peer public key
    EVP_PKEY* pubKey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr, peerPublicKey.data(), peerPublicKey.size());
    
    if (!privKey || !pubKey) {
        if (privKey) EVP_PKEY_free(privKey);
        if (pubKey) EVP_PKEY_free(pubKey);
        return {};
    }
    
    // Derive shared secret
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(privKey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(privKey);
        EVP_PKEY_free(pubKey);
        return {};
    }
    
    if (EVP_PKEY_derive_init(ctx) != 1 ||
        EVP_PKEY_derive_set_peer(ctx, pubKey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(privKey);
        EVP_PKEY_free(pubKey);
        return {};
    }
    
    size_t secretLen = 32;
    std::vector<uint8_t> sharedSecret(secretLen);
    if (EVP_PKEY_derive(ctx, sharedSecret.data(), &secretLen) != 1) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(privKey);
        EVP_PKEY_free(pubKey);
        return {};
    }
    
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(privKey);
    EVP_PKEY_free(pubKey);
    
    return sharedSecret;
}

std::vector<uint8_t> KeyManager::deriveSessionKeyFromShared(
    const std::vector<uint8_t>& sharedSecret,
    const std::string& context,
    bool isInitiator) {
    
    // Use HKDF to derive session key
    std::vector<uint8_t> sessionKey(32);
    
    // Info string includes role to ensure different keys for each direction
    std::string info = context + (isInitiator ? "-initiator" : "-responder");
    
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!ctx) {
        return {};
    }
    
    if (EVP_PKEY_derive_init(ctx) != 1 ||
        EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) != 1 ||
        EVP_PKEY_CTX_set1_hkdf_key(ctx, sharedSecret.data(), sharedSecret.size()) != 1 ||
        EVP_PKEY_CTX_add1_hkdf_info(ctx, 
            reinterpret_cast<const unsigned char*>(info.c_str()), info.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }
    
    size_t keyLen = 32;
    if (EVP_PKEY_derive(ctx, sessionKey.data(), &keyLen) != 1) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }
    EVP_PKEY_CTX_free(ctx);
    
    return sessionKey;
}

std::vector<uint8_t> KeyManager::deriveKeyFromSessionCode(
    const std::string& sessionCode,
    const std::vector<uint8_t>& salt) {
    
    return Crypto::deriveKeyFromSessionCode(sessionCode, salt);
}

} // namespace SentinelFS
