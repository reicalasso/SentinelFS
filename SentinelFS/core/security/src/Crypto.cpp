#include "Crypto.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/crypto.h>  // For CRYPTO_memcmp, OPENSSL_cleanse
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace SentinelFS {

std::vector<uint8_t> Crypto::generateKey() {
    auto& logger = SentinelFS::Logger::instance();
    auto& metrics = SentinelFS::MetricsCollector::instance();
    
    logger.log(SentinelFS::LogLevel::DEBUG, "Generating encryption key", "Crypto");
    
    std::vector<uint8_t> key(KEY_SIZE);
    if (RAND_bytes(key.data(), KEY_SIZE) != 1) {
        logger.log(SentinelFS::LogLevel::ERROR, "Failed to generate random key", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Failed to generate random key");
    }
    return key;
}

std::vector<uint8_t> Crypto::generateIV() {
    std::vector<uint8_t> iv(IV_SIZE);
    if (RAND_bytes(iv.data(), IV_SIZE) != 1) {
        throw std::runtime_error("Failed to generate random IV");
    }
    return iv;
}

std::vector<uint8_t> Crypto::addPadding(const std::vector<uint8_t>& data) {
    size_t padding_length = BLOCK_SIZE - (data.size() % BLOCK_SIZE);
    std::vector<uint8_t> padded = data;
    padded.insert(padded.end(), padding_length, static_cast<uint8_t>(padding_length));
    return padded;
}

std::vector<uint8_t> Crypto::removePadding(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("Cannot remove padding from empty data");
    }
    
    uint8_t padding_length = data.back();
    if (padding_length > BLOCK_SIZE || padding_length > data.size()) {
        throw std::runtime_error("Invalid padding");
    }
    
    // Verify all padding bytes are correct
    for (size_t i = data.size() - padding_length; i < data.size(); ++i) {
        if (data[i] != padding_length) {
            throw std::runtime_error("Invalid padding bytes");
        }
    }
    
    return std::vector<uint8_t>(data.begin(), data.end() - padding_length);
}

std::vector<uint8_t> Crypto::encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv
) {
    auto& logger = SentinelFS::Logger::instance();
    auto& metrics = SentinelFS::MetricsCollector::instance();
    
    if (key.size() != KEY_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid key size for encryption", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid key size");
    }
    if (iv.size() != IV_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid IV size for encryption", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid IV size");
    }
    
    // Add PKCS7 padding
    auto padded = addPadding(plaintext);
    
    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize encryption");
    }
    
    // Disable automatic padding (we handle it manually)
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    
    // Allocate output buffer
    std::vector<uint8_t> ciphertext(padded.size() + BLOCK_SIZE);
    int len = 0;
    int ciphertext_len = 0;
    
    // Encrypt
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, padded.data(), padded.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Encryption failed");
    }
    ciphertext_len = len;
    
    // Finalize
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Encryption finalization failed");
    }
    ciphertext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::vector<uint8_t> Crypto::decrypt(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv
) {
    auto& logger = SentinelFS::Logger::instance();
    auto& metrics = SentinelFS::MetricsCollector::instance();
    
    if (key.size() != KEY_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid key size for decryption", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid key size");
    }
    if (iv.size() != IV_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid IV size for decryption", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid IV size");
    }
    if (ciphertext.size() % BLOCK_SIZE != 0) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid ciphertext size for decryption", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid ciphertext size");
    }
    
    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    // Initialize decryption
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }
    
    // Disable automatic padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    
    // Allocate output buffer
    std::vector<uint8_t> plaintext(ciphertext.size() + BLOCK_SIZE);
    int len = 0;
    int plaintext_len = 0;
    
    // Decrypt
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption failed");
    }
    plaintext_len = len;
    
    // Finalize
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption finalization failed");
    }
    plaintext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    
    plaintext.resize(plaintext_len);
    
    // Remove PKCS7 padding
    return removePadding(plaintext);
}

std::vector<uint8_t> Crypto::deriveKeyFromSessionCode(
    const std::string& sessionCode,
    const std::vector<uint8_t>& salt,
    int iterations
) {
    auto& logger = SentinelFS::Logger::instance();
    auto& metrics = SentinelFS::MetricsCollector::instance();
    
    logger.log(SentinelFS::LogLevel::DEBUG, "Deriving key from session code (legacy)", "Crypto");
    
    std::vector<uint8_t> key(KEY_SIZE);
    
    // Use PBKDF2-HMAC-SHA256 for key derivation
    if (PKCS5_PBKDF2_HMAC(
        sessionCode.c_str(),
        sessionCode.length(),
        salt.data(),
        salt.size(),
        iterations,
        EVP_sha256(),
        KEY_SIZE,
        key.data()
    ) != 1) {
        logger.log(SentinelFS::LogLevel::ERROR, "Key derivation from session code failed", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Key derivation failed");
    }
    
    return key;
}

DerivedKeys Crypto::deriveKeyPair(
    const std::string& sessionCode,
    const std::vector<uint8_t>& salt,
    int iterations
) {
    auto& logger = SentinelFS::Logger::instance();
    auto& metrics = SentinelFS::MetricsCollector::instance();
    
    logger.log(SentinelFS::LogLevel::DEBUG, "Deriving key pair from session code", "Crypto");
    
    // Derive 64 bytes: 32 for encryption, 32 for MAC
    constexpr size_t DERIVED_SIZE = 64;
    std::vector<uint8_t> derivedKey(DERIVED_SIZE);
    
    // Use PBKDF2-HMAC-SHA256 for key derivation
    if (PKCS5_PBKDF2_HMAC(
        sessionCode.c_str(),
        sessionCode.length(),
        salt.data(),
        salt.size(),
        iterations,
        EVP_sha256(),
        DERIVED_SIZE,
        derivedKey.data()
    ) != 1) {
        logger.log(SentinelFS::LogLevel::ERROR, "Key pair derivation failed", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Key derivation failed");
    }
    
    // Split into encryption and MAC keys
    DerivedKeys keys;
    keys.encKey.assign(derivedKey.begin(), derivedKey.begin() + KEY_SIZE);
    keys.macKey.assign(derivedKey.begin() + KEY_SIZE, derivedKey.end());
    
    // Securely clear the combined key
    OPENSSL_cleanse(derivedKey.data(), derivedKey.size());
    
    return keys;
}

bool Crypto::constantTimeCompare(
    const std::vector<uint8_t>& a,
    const std::vector<uint8_t>& b
) {
    if (a.size() != b.size()) {
        return false;
    }
    
    // Use OpenSSL's constant-time comparison
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

// ==================== Argon2id Implementation ====================

bool Crypto::isArgon2Available() {
    // Argon2 requires OpenSSL 3.2+ with specific provider support
    // For compatibility, we always use PBKDF2 with high iterations
    // This can be updated when Argon2 support is more widespread
    return false;
}

DerivedKeys Crypto::deriveKeyPairArgon2(
    const std::string& sessionCode,
    const std::vector<uint8_t>& salt
) {
    auto& logger = SentinelFS::Logger::instance();
    
    // Argon2id is not widely available in OpenSSL yet
    // Use PBKDF2 with very high iteration count as secure fallback
    // 310,000 iterations is OWASP recommendation for PBKDF2-SHA256
    logger.log(SentinelFS::LogLevel::DEBUG, 
        "Using PBKDF2 with high iterations (Argon2 not available)", "Crypto");
    
    return deriveKeyPair(sessionCode, salt, 310000);
}

// ==================== AES-GCM AEAD Implementation ====================

std::vector<uint8_t> Crypto::generateGcmNonce() {
    std::vector<uint8_t> nonce(GCM_IV_SIZE);
    if (RAND_bytes(nonce.data(), GCM_IV_SIZE) != 1) {
        throw std::runtime_error("Failed to generate GCM nonce");
    }
    return nonce;
}

std::vector<uint8_t> Crypto::encryptGcm(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce,
    const std::vector<uint8_t>& aad
) {
    auto& logger = SentinelFS::Logger::instance();
    auto& metrics = SentinelFS::MetricsCollector::instance();
    
    if (key.size() != KEY_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid key size for GCM encryption", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid key size");
    }
    if (nonce.size() != GCM_IV_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid nonce size for GCM", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid nonce size (must be 12 bytes)");
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    // Initialize AES-256-GCM encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize GCM");
    }
    
    // Set nonce length
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_SIZE, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set GCM IV length");
    }
    
    // Set key and nonce
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set GCM key/nonce");
    }
    
    int len = 0;
    
    // Add AAD (Additional Authenticated Data) if provided
    if (!aad.empty()) {
        if (EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), aad.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to add AAD");
        }
    }
    
    // Encrypt plaintext
    std::vector<uint8_t> ciphertext(plaintext.size() + GCM_TAG_SIZE);
    int ciphertext_len = 0;
    
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("GCM encryption failed");
    }
    ciphertext_len = len;
    
    // Finalize
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("GCM finalization failed");
    }
    ciphertext_len += len;
    
    // Get authentication tag and append to ciphertext
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, ciphertext.data() + ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to get GCM tag");
    }
    ciphertext_len += GCM_TAG_SIZE;
    
    EVP_CIPHER_CTX_free(ctx);
    
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::vector<uint8_t> Crypto::decryptGcm(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce,
    const std::vector<uint8_t>& aad
) {
    auto& logger = SentinelFS::Logger::instance();
    auto& metrics = SentinelFS::MetricsCollector::instance();
    
    if (key.size() != KEY_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid key size for GCM decryption", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid key size");
    }
    if (nonce.size() != GCM_IV_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Invalid nonce size for GCM", "Crypto");
        metrics.incrementEncryptionErrors();
        throw std::runtime_error("Invalid nonce size");
    }
    if (ciphertext.size() < GCM_TAG_SIZE) {
        logger.log(SentinelFS::LogLevel::ERROR, "Ciphertext too short for GCM", "Crypto");
        metrics.incrementEncryptionErrors();
        return {};  // Return empty on auth failure
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    // Initialize AES-256-GCM decryption
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize GCM decryption");
    }
    
    // Set nonce length
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_SIZE, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set GCM IV length");
    }
    
    // Set key and nonce
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set GCM key/nonce");
    }
    
    int len = 0;
    
    // Add AAD if provided
    if (!aad.empty()) {
        if (EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), aad.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to add AAD");
        }
    }
    
    // Separate ciphertext and tag
    size_t actualCiphertextLen = ciphertext.size() - GCM_TAG_SIZE;
    
    // Decrypt
    std::vector<uint8_t> plaintext(actualCiphertextLen);
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), actualCiphertextLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};  // Decryption failed
    }
    int plaintext_len = len;
    
    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, 
                           const_cast<uint8_t*>(ciphertext.data() + actualCiphertextLen)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    // Verify tag and finalize - THIS IS THE AUTH CHECK
    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);
    
    if (ret <= 0) {
        // Authentication failed!
        logger.log(SentinelFS::LogLevel::WARN, "GCM authentication failed", "Crypto");
        metrics.incrementEncryptionErrors();
        return {};  // Return empty vector on auth failure
    }
    
    plaintext_len += len;
    plaintext.resize(plaintext_len);
    return plaintext;
}

std::vector<uint8_t> Crypto::hmacSHA256(
    const std::vector<uint8_t>& message,
    const std::vector<uint8_t>& key
) {
    std::vector<uint8_t> hmac(32); // SHA256 output size
    unsigned int hmac_len = 0;
    
    if (HMAC(
        EVP_sha256(),
        key.data(),
        key.size(),
        message.data(),
        message.size(),
        hmac.data(),
        &hmac_len
    ) == nullptr) {
        throw std::runtime_error("HMAC computation failed");
    }
    
    hmac.resize(hmac_len);
    return hmac;
}

std::string Crypto::toHex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<uint8_t> Crypto::fromHex(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        throw std::runtime_error("Invalid hex string length");
    }
    
    std::vector<uint8_t> data;
    data.reserve(hex.length() / 2);
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
        data.push_back(byte);
    }
    
    return data;
}

// EncryptedMessage implementation

std::vector<uint8_t> EncryptedMessage::getAuthenticatedData() const {
    std::vector<uint8_t> authData;
    
    // For GCM: AAD = version || sequence (IV is handled separately by GCM)
    // For CBC: HMAC covers version || sequence || IV || ciphertext
    if (isAead()) {
        authData.reserve(1 + 8);
    } else {
        authData.reserve(1 + 8 + iv.size() + ciphertext.size());
    }
    
    // Version (1 byte)
    authData.push_back(version);
    
    // Sequence (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        authData.push_back(static_cast<uint8_t>((sequence >> (i * 8)) & 0xFF));
    }
    
    // For CBC mode, also include IV and ciphertext in HMAC
    if (!isAead()) {
        authData.insert(authData.end(), iv.begin(), iv.end());
        authData.insert(authData.end(), ciphertext.begin(), ciphertext.end());
    }
    
    return authData;
}

std::vector<uint8_t> EncryptedMessage::serialize() const {
    std::vector<uint8_t> result;
    
    if (isAead()) {
        // GCM Format: [Version (1)] [Sequence (8)] [Nonce (12)] [Ciphertext+Tag]
        result.reserve(1 + 8 + iv.size() + ciphertext.size());
        
        result.push_back(version);
        
        for (int i = 7; i >= 0; --i) {
            result.push_back(static_cast<uint8_t>((sequence >> (i * 8)) & 0xFF));
        }
        
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        // Note: For GCM, ciphertext already includes the 16-byte auth tag
    } else {
        // CBC Format: [Version (1)] [Sequence (8)] [IV (16)] [Ciphertext] [HMAC (32)]
        result.reserve(1 + 8 + iv.size() + ciphertext.size() + hmac.size());
        
        result.push_back(version);
        
        for (int i = 7; i >= 0; --i) {
            result.push_back(static_cast<uint8_t>((sequence >> (i * 8)) & 0xFF));
        }
        
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        result.insert(result.end(), hmac.begin(), hmac.end());
    }
    
    return result;
}

EncryptedMessage EncryptedMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("Empty encrypted message");
    }
    
    EncryptedMessage msg;
    size_t offset = 0;
    
    // Version (1 byte)
    msg.version = data[offset++];
    
    // Determine format based on version
    bool isGcm = (msg.version >= VERSION_GCM);
    
    size_t ivSize = isGcm ? Crypto::GCM_IV_SIZE : Crypto::IV_SIZE;
    size_t minSize = isGcm 
        ? (1 + 8 + Crypto::GCM_IV_SIZE + Crypto::GCM_TAG_SIZE)  // GCM minimum
        : (1 + 8 + Crypto::IV_SIZE + Crypto::BLOCK_SIZE + 32);  // CBC minimum
    
    if (data.size() < minSize) {
        throw std::runtime_error("Invalid encrypted message size");
    }
    
    // Sequence (8 bytes, big-endian)
    msg.sequence = 0;
    for (int i = 0; i < 8; ++i) {
        msg.sequence = (msg.sequence << 8) | data[offset++];
    }
    
    // IV/Nonce
    msg.iv.assign(data.begin() + offset, data.begin() + offset + ivSize);
    offset += ivSize;
    
    if (isGcm) {
        // GCM: rest is ciphertext (includes auth tag at end)
        msg.ciphertext.assign(data.begin() + offset, data.end());
        // hmac is empty for GCM
    } else {
        // CBC: HMAC is last 32 bytes
        msg.hmac.assign(data.end() - 32, data.end());
        msg.ciphertext.assign(data.begin() + offset, data.end() - 32);
    }
    
    return msg;
}

} // namespace SentinelFS
