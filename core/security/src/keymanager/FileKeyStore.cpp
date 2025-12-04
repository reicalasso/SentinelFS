/**
 * @file FileKeyStore.cpp
 * @brief Encrypted file-based key storage implementation
 */

#include "KeyManager.h"
#include "Logger.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace SentinelFS {

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

} // namespace SentinelFS
