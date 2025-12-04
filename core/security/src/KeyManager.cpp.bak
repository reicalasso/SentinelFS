#include "KeyManager.h"
#include "Crypto.h"
#include "Logger.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/kdf.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>

// Note: For full Ed25519/X25519 support, this implementation uses OpenSSL 3.0+
// or requires linking against libsodium for the curve operations.
// This is a reference implementation showing the key management structure.

namespace SentinelFS {

//=====================================================
// KeyInfo Implementation
//=====================================================

bool KeyInfo::isExpired() const {
    if (expires.time_since_epoch().count() == 0) {
        return false;  // No expiration
    }
    return std::chrono::system_clock::now() > expires;
}

bool KeyInfo::isValid() const {
    return !compromised && !isExpired();
}

std::string KeyInfo::toString() const {
    std::stringstream ss;
    ss << "KeyInfo{id=" << keyId.substr(0, 8) << "..., type=" 
       << static_cast<int>(type) << ", algorithm=" << algorithm << "}";
    return ss.str();
}

//=====================================================
// SessionKey Implementation
//=====================================================

bool SessionKey::needsRotation() const {
    // Check time-based expiration
    if (std::chrono::system_clock::now() > expires) {
        return true;
    }
    
    // Check usage-based thresholds
    if (bytesEncrypted > MAX_BYTES) {
        return true;
    }
    
    if (messagesEncrypted > MAX_MESSAGES) {
        return true;
    }
    
    return false;
}

//=====================================================
// IdentityKeyPair Implementation
//=====================================================

std::string IdentityKeyPair::computeFingerprint() const {
    // Compute SHA256 of public key
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(publicKey.data(), publicKey.size(), hash);
    
    // Format as human-readable fingerprint (like SSH)
    // Example: "SHA256:Ab3C:D4eF:5678:..."
    std::stringstream ss;
    ss << "SHA256:";
    for (int i = 0; i < 16; i += 2) {
        if (i > 0) ss << ":";
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i + 1]);
    }
    return ss.str();
}

//=====================================================
// FileKeyStore Implementation
//=====================================================

FileKeyStore::FileKeyStore(const std::string& storagePath,
                           const std::string& masterPassword)
    : storagePath_(storagePath)
{
    // Derive master key from password
    masterKey_ = deriveMasterKey(masterPassword);
    
    // Ensure storage directory exists with secure permissions
    struct stat st;
    if (stat(storagePath_.c_str(), &st) != 0) {
        mkdir(storagePath_.c_str(), 0700);
    }
}

FileKeyStore::~FileKeyStore() {
    // Securely clear master key from memory
    if (!masterKey_.empty()) {
        OPENSSL_cleanse(masterKey_.data(), masterKey_.size());
    }
}

std::vector<uint8_t> FileKeyStore::deriveMasterKey(const std::string& password) {
    // Use PBKDF2 to derive master key from password
    std::vector<uint8_t> salt(16);
    std::string saltFile = storagePath_ + "/.salt";
    
    std::ifstream saltIn(saltFile, std::ios::binary);
    if (saltIn.good()) {
        saltIn.read(reinterpret_cast<char*>(salt.data()), salt.size());
    } else {
        // Generate new salt
        RAND_bytes(salt.data(), salt.size());
        std::ofstream saltOut(saltFile, std::ios::binary);
        saltOut.write(reinterpret_cast<char*>(salt.data()), salt.size());
        chmod(saltFile.c_str(), 0600);
    }
    
    std::vector<uint8_t> key(32);
    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                      salt.data(), salt.size(),
                      100000, EVP_sha256(),
                      key.size(), key.data());
    
    return key;
}

std::string FileKeyStore::keyFilePath(const std::string& keyId) const {
    return storagePath_ + "/" + keyId + ".key";
}

std::string FileKeyStore::metadataFilePath(const std::string& keyId) const {
    return storagePath_ + "/" + keyId + ".meta";
}

std::vector<uint8_t> FileKeyStore::encryptKey(const std::vector<uint8_t>& plainKey) {
    // Generate random IV
    std::vector<uint8_t> iv(16);
    RAND_bytes(iv.data(), iv.size());
    
    // Encrypt using AES-256-GCM
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, 
                       masterKey_.data(), iv.data());
    
    std::vector<uint8_t> ciphertext(plainKey.size() + 16);  // Room for tag
    int len = 0;
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plainKey.data(), plainKey.size());
    int cipherLen = len;
    
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    cipherLen += len;
    
    // Get authentication tag
    std::vector<uint8_t> tag(16);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
    
    EVP_CIPHER_CTX_free(ctx);
    
    // Return: IV || ciphertext || tag
    std::vector<uint8_t> result;
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + cipherLen);
    result.insert(result.end(), tag.begin(), tag.end());
    
    return result;
}

std::vector<uint8_t> FileKeyStore::decryptKey(const std::vector<uint8_t>& encryptedKey) {
    if (encryptedKey.size() < 32) {  // IV (16) + tag (16) minimum
        return {};
    }
    
    // Extract IV, ciphertext, tag
    std::vector<uint8_t> iv(encryptedKey.begin(), encryptedKey.begin() + 16);
    std::vector<uint8_t> tag(encryptedKey.end() - 16, encryptedKey.end());
    std::vector<uint8_t> ciphertext(encryptedKey.begin() + 16, encryptedKey.end() - 16);
    
    // Decrypt
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                       masterKey_.data(), iv.data());
    
    std::vector<uint8_t> plaintext(ciphertext.size());
    int len = 0;
    EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size());
    int plainLen = len;
    
    // Set expected tag
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag.data());
    
    // Verify tag and finalize
    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);
    
    if (ret <= 0) {
        // Authentication failed
        return {};
    }
    
    plaintext.resize(plainLen + len);
    return plaintext;
}

bool FileKeyStore::store(const std::string& keyId,
                         const std::vector<uint8_t>& keyData,
                         const KeyInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Encrypt key data
    std::vector<uint8_t> encrypted = encryptKey(keyData);
    
    // Write encrypted key
    std::string keyPath = keyFilePath(keyId);
    std::ofstream keyFile(keyPath, std::ios::binary);
    if (!keyFile.good()) {
        return false;
    }
    keyFile.write(reinterpret_cast<char*>(encrypted.data()), encrypted.size());
    chmod(keyPath.c_str(), 0600);
    
    // Write metadata
    std::string metaPath = metadataFilePath(keyId);
    std::ofstream metaFile(metaPath);
    if (!metaFile.good()) {
        return false;
    }
    
    auto createdTime = std::chrono::duration_cast<std::chrono::seconds>(
        info.created.time_since_epoch()).count();
    auto expiresTime = std::chrono::duration_cast<std::chrono::seconds>(
        info.expires.time_since_epoch()).count();
    
    metaFile << "type=" << static_cast<int>(info.type) << "\n"
             << "algorithm=" << info.algorithm << "\n"
             << "created=" << createdTime << "\n"
             << "expires=" << expiresTime << "\n"
             << "peerId=" << info.peerId << "\n"
             << "compromised=" << (info.compromised ? "1" : "0") << "\n";
    
    chmod(metaPath.c_str(), 0600);
    
    return true;
}

std::vector<uint8_t> FileKeyStore::load(const std::string& keyId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string keyPath = keyFilePath(keyId);
    std::ifstream keyFile(keyPath, std::ios::binary);
    if (!keyFile.good()) {
        return {};
    }
    
    std::vector<uint8_t> encrypted(
        (std::istreambuf_iterator<char>(keyFile)),
        std::istreambuf_iterator<char>());
    
    return decryptKey(encrypted);
}

bool FileKeyStore::remove(const std::string& keyId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Securely overwrite before removing
    std::string keyPath = keyFilePath(keyId);
    std::string metaPath = metadataFilePath(keyId);
    
    // Overwrite with random data
    std::ifstream keyFileIn(keyPath, std::ios::binary | std::ios::ate);
    if (keyFileIn.good()) {
        size_t size = keyFileIn.tellg();
        keyFileIn.close();
        
        std::vector<uint8_t> random(size);
        RAND_bytes(random.data(), random.size());
        
        std::ofstream keyFileOut(keyPath, std::ios::binary);
        keyFileOut.write(reinterpret_cast<char*>(random.data()), random.size());
    }
    
    std::remove(keyPath.c_str());
    std::remove(metaPath.c_str());
    
    return true;
}

std::vector<KeyInfo> FileKeyStore::list(KeyType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<KeyInfo> results;
    
    // Iterate through .meta files
    // Note: Production code should use proper directory iteration
    // This is simplified for the example
    
    return results;
}

bool FileKeyStore::exists(const std::string& keyId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    struct stat st;
    return stat(keyFilePath(keyId).c_str(), &st) == 0;
}

bool FileKeyStore::changePassword(const std::string& oldPassword,
                                   const std::string& newPassword) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Verify old password
    auto testKey = deriveMasterKey(oldPassword);
    if (testKey != masterKey_) {
        return false;
    }
    
    // Re-encrypt all keys with new password
    auto newMasterKey = deriveMasterKey(newPassword);
    
    // TODO: Re-encrypt all stored keys
    
    masterKey_ = newMasterKey;
    return true;
}

//=====================================================
// KeyManager Implementation
//=====================================================

KeyManager::KeyManager(std::shared_ptr<IKeyStore> keyStore)
    : keyStore_(std::move(keyStore))
{
}

KeyManager::~KeyManager() {
    // Securely clear sensitive data
    if (identityKey_) {
        if (!identityKey_->privateKey.empty()) {
            OPENSSL_cleanse(identityKey_->privateKey.data(), 
                           identityKey_->privateKey.size());
        }
    }
    
    for (auto& [peerId, session] : sessionKeys_) {
        if (!session.key.empty()) {
            OPENSSL_cleanse(session.key.data(), session.key.size());
        }
    }
}

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
    EVP_DigestSignInit(mdctx, nullptr, nullptr, nullptr, pkey);
    
    size_t sigLen = 64;
    std::vector<uint8_t> signature(sigLen);
    
    EVP_DigestSign(mdctx, signature.data(), &sigLen, data.data(), data.size());
    
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
    EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, pkey);
    
    int result = EVP_DigestVerify(mdctx, signature.data(), signature.size(),
                                   data.data(), data.size());
    
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    
    return result == 1;
}

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
    
    sessionKeys_[peerId] = std::move(session);
    
    logger.log(LogLevel::INFO, "Derived session key for peer: " + peerId, "KeyManager");
    
    return sessionKeys_[peerId].keyId;
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
    EVP_PKEY_keygen_init(ctx);
    
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_keygen(ctx, &pkey);
    EVP_PKEY_CTX_free(ctx);
    
    std::vector<uint8_t> publicKey(32);
    std::vector<uint8_t> privateKey(32);
    
    size_t len = 32;
    EVP_PKEY_get_raw_public_key(pkey, publicKey.data(), &len);
    EVP_PKEY_get_raw_private_key(pkey, privateKey.data(), &len);
    
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
    EVP_PKEY_derive_init(ctx);
    EVP_PKEY_derive_set_peer(ctx, pubKey);
    
    size_t secretLen = 32;
    std::vector<uint8_t> sharedSecret(secretLen);
    EVP_PKEY_derive(ctx, sharedSecret.data(), &secretLen);
    
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
    EVP_PKEY_derive_init(ctx);
    EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256());
    EVP_PKEY_CTX_set1_hkdf_key(ctx, sharedSecret.data(), sharedSecret.size());
    EVP_PKEY_CTX_add1_hkdf_info(ctx, 
        reinterpret_cast<const unsigned char*>(info.c_str()), info.size());
    
    size_t keyLen = 32;
    EVP_PKEY_derive(ctx, sessionKey.data(), &keyLen);
    EVP_PKEY_CTX_free(ctx);
    
    return sessionKey;
}

std::vector<uint8_t> KeyManager::deriveKeyFromSessionCode(
    const std::string& sessionCode,
    const std::vector<uint8_t>& salt) {
    
    return Crypto::deriveKeyFromSessionCode(sessionCode, salt);
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
