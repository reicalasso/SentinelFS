/**
 * @file IdentityKeyManager.cpp
 * @brief Identity key management (generation, loading, signing, verification)
 */

#include "KeyManager.h"
#include "Crypto.h"
#include "Logger.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace SentinelFS {

bool KeyManager::generateIdentityKey(const std::string& deviceName) {
    auto& logger = Logger::instance();
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate Ed25519 keypair using OpenSSL 3.0+
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!ctx) {
        logger.log(LogLevel::ERROR, "Failed to create key context", "KeyManager");
        return false;
    }
    
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        logger.log(LogLevel::ERROR, "Failed to init keygen", "KeyManager");
        return false;
    }
    
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        logger.log(LogLevel::ERROR, "Failed to generate key", "KeyManager");
        return false;
    }
    
    EVP_PKEY_CTX_free(ctx);
    
    // Extract keys
    identityKey_ = std::make_unique<IdentityKeyPair>();
    identityKey_->deviceName = deviceName;
    identityKey_->created = std::chrono::system_clock::now();
    
    // Get public key
    size_t pubLen = 32;
    identityKey_->publicKey.resize(pubLen);
    EVP_PKEY_get_raw_public_key(pkey, identityKey_->publicKey.data(), &pubLen);
    
    // Get private key
    size_t privLen = 64;
    identityKey_->privateKey.resize(privLen);
    EVP_PKEY_get_raw_private_key(pkey, identityKey_->privateKey.data(), &privLen);
    identityKey_->privateKey.resize(privLen);
    
    EVP_PKEY_free(pkey);
    
    // Compute key ID and fingerprint
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(identityKey_->publicKey.data(), identityKey_->publicKey.size(), hash);
    identityKey_->keyId = Crypto::toHex(std::vector<uint8_t>(hash, hash + 16));
    identityKey_->fingerprint = identityKey_->computeFingerprint();
    
    // Store the identity key
    KeyInfo info;
    info.keyId = identityKey_->keyId;
    info.type = KeyType::IDENTITY_PRIVATE;
    info.created = identityKey_->created;
    info.algorithm = "Ed25519";
    
    // Combine public and private for storage
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), identityKey_->publicKey.begin(), 
                   identityKey_->publicKey.end());
    combined.insert(combined.end(), identityKey_->privateKey.begin(),
                   identityKey_->privateKey.end());
    
    if (!keyStore_->store(identityKey_->keyId, combined, info)) {
        logger.log(LogLevel::ERROR, "Failed to store identity key", "KeyManager");
        identityKey_.reset();
        return false;
    }
    
    logger.log(LogLevel::INFO, "Generated new identity key: " + 
               identityKey_->fingerprint, "KeyManager");
    
    return true;
}

bool KeyManager::loadIdentityKey() {
    auto& logger = Logger::instance();
    std::lock_guard<std::mutex> lock(mutex_);
    
    // List identity keys in store
    auto keys = keyStore_->list(KeyType::IDENTITY_PRIVATE);
    if (keys.empty()) {
        return false;
    }
    
    // Load the first (should be only) identity key
    auto keyData = keyStore_->load(keys[0].keyId);
    if (keyData.empty()) {
        logger.log(LogLevel::ERROR, "Failed to load identity key", "KeyManager");
        return false;
    }
    
    identityKey_ = std::make_unique<IdentityKeyPair>();
    identityKey_->keyId = keys[0].keyId;
    identityKey_->created = keys[0].created;
    
    // Split public and private
    if (keyData.size() >= 32) {
        identityKey_->publicKey.assign(keyData.begin(), keyData.begin() + 32);
        identityKey_->privateKey.assign(keyData.begin() + 32, keyData.end());
    }
    
    identityKey_->fingerprint = identityKey_->computeFingerprint();
    
    logger.log(LogLevel::INFO, "Loaded identity key: " + 
               identityKey_->fingerprint, "KeyManager");
    
    return true;
}

bool KeyManager::hasIdentityKey() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return identityKey_ != nullptr;
}

std::string KeyManager::getLocalKeyId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return identityKey_ ? identityKey_->keyId : "";
}

std::vector<uint8_t> KeyManager::getPublicKey() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return identityKey_ ? identityKey_->publicKey : std::vector<uint8_t>{};
}

std::string KeyManager::getFingerprint() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return identityKey_ ? identityKey_->fingerprint : "";
}

std::vector<uint8_t> KeyManager::sign(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& logger = Logger::instance();
    
    if (!identityKey_) {
        logger.log(LogLevel::ERROR, "No identity key for signing", "KeyManager");
        return {};
    }
    
    // Create signing key
    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr,
        identityKey_->privateKey.data(),
        identityKey_->privateKey.size());
    
    if (!pkey) {
        return {};
    }
    
    // Sign
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(pkey);
        return {};
    }
    
    if (EVP_DigestSignInit(mdctx, nullptr, nullptr, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return {};
    }
    
    size_t sigLen = 64;
    std::vector<uint8_t> signature(sigLen);
    
    if (EVP_DigestSign(mdctx, signature.data(), &sigLen, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return {};
    }
    
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    
    signature.resize(sigLen);
    return signature;
}

bool KeyManager::verify(const std::vector<uint8_t>& data,
                        const std::vector<uint8_t>& signature,
                        const std::vector<uint8_t>& peerPublicKey) {
    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr,
        peerPublicKey.data(), peerPublicKey.size());
    
    if (!pkey) {
        return false;
    }
    
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(pkey);
        return false;
    }
    
    if (EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return false;
    }
    
    int result = EVP_DigestVerify(mdctx, signature.data(), signature.size(),
                                   data.data(), data.size());
    
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    
    return result == 1;
}

std::vector<uint8_t> KeyManager::exportIdentity(const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!identityKey_) {
        return {};
    }
    
    // Create backup structure
    std::vector<uint8_t> backup;
    
    // Version byte
    backup.push_back(0x01);
    
    // Device name length + data
    uint16_t nameLen = identityKey_->deviceName.size();
    backup.push_back(nameLen >> 8);
    backup.push_back(nameLen & 0xFF);
    backup.insert(backup.end(), identityKey_->deviceName.begin(),
                  identityKey_->deviceName.end());
    
    // Public key
    backup.insert(backup.end(), identityKey_->publicKey.begin(),
                  identityKey_->publicKey.end());
    
    // Private key
    backup.insert(backup.end(), identityKey_->privateKey.begin(),
                  identityKey_->privateKey.end());
    
    // Encrypt with password
    std::vector<uint8_t> salt(16);
    RAND_bytes(salt.data(), salt.size());
    
    auto key = Crypto::deriveKeyFromSessionCode(password, salt, 200000);
    auto iv = Crypto::generateIV();
    auto encrypted = Crypto::encrypt(backup, key, iv);
    
    // Result: salt || IV || encrypted
    std::vector<uint8_t> result;
    result.insert(result.end(), salt.begin(), salt.end());
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), encrypted.begin(), encrypted.end());
    
    return result;
}

bool KeyManager::importIdentity(const std::vector<uint8_t>& backupData,
                                 const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& logger = Logger::instance();
    
    if (backupData.size() < 48) {  // salt + IV minimum
        return false;
    }
    
    // Extract salt, IV, encrypted data
    std::vector<uint8_t> salt(backupData.begin(), backupData.begin() + 16);
    std::vector<uint8_t> iv(backupData.begin() + 16, backupData.begin() + 32);
    std::vector<uint8_t> encrypted(backupData.begin() + 32, backupData.end());
    
    // Decrypt
    auto key = Crypto::deriveKeyFromSessionCode(password, salt, 200000);
    std::vector<uint8_t> decrypted;
    
    try {
        decrypted = Crypto::decrypt(encrypted, key, iv);
    } catch (...) {
        logger.log(LogLevel::ERROR, "Failed to decrypt backup (wrong password?)", "KeyManager");
        return false;
    }
    
    if (decrypted.empty() || decrypted[0] != 0x01) {
        return false;
    }
    
    // Parse backup structure
    size_t offset = 1;
    uint16_t nameLen = (decrypted[offset] << 8) | decrypted[offset + 1];
    offset += 2;
    
    std::string deviceName(decrypted.begin() + offset,
                           decrypted.begin() + offset + nameLen);
    offset += nameLen;
    
    std::vector<uint8_t> publicKey(decrypted.begin() + offset,
                                    decrypted.begin() + offset + 32);
    offset += 32;
    
    std::vector<uint8_t> privateKey(decrypted.begin() + offset, decrypted.end());
    
    // Create identity key
    identityKey_ = std::make_unique<IdentityKeyPair>();
    identityKey_->deviceName = deviceName;
    identityKey_->publicKey = publicKey;
    identityKey_->privateKey = privateKey;
    identityKey_->created = std::chrono::system_clock::now();
    
    // Compute key ID and fingerprint
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(publicKey.data(), publicKey.size(), hash);
    identityKey_->keyId = Crypto::toHex(std::vector<uint8_t>(hash, hash + 16));
    identityKey_->fingerprint = identityKey_->computeFingerprint();
    
    // Store
    KeyInfo info;
    info.keyId = identityKey_->keyId;
    info.type = KeyType::IDENTITY_PRIVATE;
    info.algorithm = "Ed25519";
    info.created = identityKey_->created;
    
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), publicKey.begin(), publicKey.end());
    combined.insert(combined.end(), privateKey.begin(), privateKey.end());
    
    keyStore_->store(info.keyId, combined, info);
    
    logger.log(LogLevel::INFO, "Imported identity key: " + 
               identityKey_->fingerprint, "KeyManager");
    
    return true;
}

} // namespace SentinelFS
