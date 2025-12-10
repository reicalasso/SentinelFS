#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <array>

namespace SentinelFS {

/**
 * @brief Derived key pair for encryption and MAC (key separation)
 */
struct DerivedKeys {
    std::vector<uint8_t> encKey;  // 32 bytes for AES-256
    std::vector<uint8_t> macKey;  // 32 bytes for HMAC-SHA256
};

/**
 * @brief Key derivation function type
 */
enum class KdfType {
    PBKDF2_SHA256,      // Legacy, compatible
    ARGON2ID            // Modern, memory-hard (recommended)
};

/**
 * @brief Cipher mode for encryption
 */
enum class CipherMode {
    AES_256_CBC,        // Legacy with HMAC
    AES_256_GCM,        // AEAD (recommended)
    CHACHA20_POLY1305   // AEAD alternative
};

/**
 * @brief Cryptographic utilities for secure data transfer
 * 
 * Supports:
 * - AES-256-CBC with HMAC (legacy)
 * - AES-256-GCM (AEAD, recommended)
 * - ChaCha20-Poly1305 (AEAD alternative)
 * - PBKDF2 and Argon2id for key derivation
 */
class Crypto {
public:
    static constexpr size_t KEY_SIZE = 32;      // 256 bits
    static constexpr size_t IV_SIZE = 16;       // 128 bits for CBC
    static constexpr size_t GCM_IV_SIZE = 12;   // 96 bits for GCM (recommended)
    static constexpr size_t GCM_TAG_SIZE = 16;  // 128 bits auth tag
    static constexpr size_t BLOCK_SIZE = 16;    // AES block size
    
    // Argon2id parameters (OWASP recommended)
    static constexpr uint32_t ARGON2_TIME_COST = 3;       // iterations
    static constexpr uint32_t ARGON2_MEMORY_COST = 65536; // 64 MB
    static constexpr uint32_t ARGON2_PARALLELISM = 4;     // threads
    
    /**
     * @brief Generate a random encryption key
     * @return 32-byte key suitable for AES-256
     */
    static std::vector<uint8_t> generateKey();
    
    /**
     * @brief Generate a random initialization vector
     * @return 16-byte IV for CBC mode
     */
    static std::vector<uint8_t> generateIV();
    
    /**
     * @brief Encrypt data using AES-256-CBC
     * @param plaintext Data to encrypt
     * @param key 32-byte encryption key
     * @param iv 16-byte initialization vector
     * @return Encrypted data with PKCS7 padding
     * @throws std::runtime_error on encryption failure
     */
    static std::vector<uint8_t> encrypt(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv
    );
    
    /**
     * @brief Decrypt data using AES-256-CBC
     * @param ciphertext Data to decrypt
     * @param key 32-byte encryption key
     * @param iv 16-byte initialization vector
     * @return Decrypted data with padding removed
     * @throws std::runtime_error on decryption failure
     */
    static std::vector<uint8_t> decrypt(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv
    );
    
    /**
     * @brief Derive a shared secret from session code (LEGACY - use deriveKeyPair instead)
     * @param sessionCode 6-character session code
     * @param salt Random salt for key derivation
     * @param iterations PBKDF2 iteration count (default: 100000)
     * @return 32-byte derived key
     * @deprecated Use deriveKeyPair for proper key separation
     */
    static std::vector<uint8_t> deriveKeyFromSessionCode(
        const std::string& sessionCode,
        const std::vector<uint8_t>& salt,
        int iterations = 100000
    );
    
    /**
     * @brief Derive separate encryption and MAC keys from session code
     * 
     * Uses PBKDF2 to derive 64 bytes, then splits into:
     * - enc_key = K[0:32] for AES-256
     * - mac_key = K[32:64] for HMAC-SHA256
     * 
     * @param sessionCode Session code
     * @param salt Random salt for key derivation
     * @param iterations PBKDF2 iteration count (default: 100000, minimum recommended)
     * @return DerivedKeys with separate encryption and MAC keys
     */
    static DerivedKeys deriveKeyPair(
        const std::string& sessionCode,
        const std::vector<uint8_t>& salt,
        int iterations = 100000
    );
    
    /**
     * @brief Derive key pair using Argon2id (recommended)
     * 
     * Memory-hard KDF resistant to GPU/ASIC attacks.
     * Uses OWASP recommended parameters.
     * 
     * @param sessionCode Session code
     * @param salt Random salt (16+ bytes recommended)
     * @return DerivedKeys with separate encryption and MAC keys
     */
    static DerivedKeys deriveKeyPairArgon2(
        const std::string& sessionCode,
        const std::vector<uint8_t>& salt
    );
    
    /**
     * @brief Check if Argon2 is available
     */
    static bool isArgon2Available();
    
    // ==================== AEAD Functions ====================
    
    /**
     * @brief Generate nonce for GCM mode
     * @return 12-byte nonce
     */
    static std::vector<uint8_t> generateGcmNonce();
    
    /**
     * @brief Encrypt using AES-256-GCM (AEAD)
     * 
     * Provides both confidentiality and authenticity in one operation.
     * No separate HMAC needed.
     * 
     * @param plaintext Data to encrypt
     * @param key 32-byte encryption key
     * @param nonce 12-byte nonce (must be unique per message)
     * @param aad Additional authenticated data (optional, not encrypted but authenticated)
     * @return Ciphertext with appended 16-byte auth tag
     */
    static std::vector<uint8_t> encryptGcm(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce,
        const std::vector<uint8_t>& aad = {}
    );
    
    /**
     * @brief Decrypt using AES-256-GCM (AEAD)
     * 
     * Verifies authenticity before returning plaintext.
     * 
     * @param ciphertext Ciphertext with appended auth tag
     * @param key 32-byte encryption key
     * @param nonce 12-byte nonce used during encryption
     * @param aad Additional authenticated data (must match encryption)
     * @return Plaintext or empty vector on auth failure
     */
    static std::vector<uint8_t> decryptGcm(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce,
        const std::vector<uint8_t>& aad = {}
    );
    
    /**
     * @brief Constant-time comparison to prevent timing attacks
     * @param a First buffer
     * @param b Second buffer
     * @return true if buffers are equal
     */
    static bool constantTimeCompare(
        const std::vector<uint8_t>& a,
        const std::vector<uint8_t>& b
    );
    
    /**
     * @brief Compute HMAC-SHA256 for message authentication
     * @param message Data to authenticate
     * @param key HMAC key
     * @return 32-byte HMAC digest
     */
    static std::vector<uint8_t> hmacSHA256(
        const std::vector<uint8_t>& message,
        const std::vector<uint8_t>& key
    );
    
    /**
     * @brief Convert bytes to hexadecimal string
     * @param data Byte array
     * @return Hex string representation
     */
    static std::string toHex(const std::vector<uint8_t>& data);
    
    /**
     * @brief Convert hexadecimal string to bytes
     * @param hex Hex string
     * @return Byte array
     */
    static std::vector<uint8_t> fromHex(const std::string& hex);

private:
    // PKCS7 padding
    static std::vector<uint8_t> addPadding(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> removePadding(const std::vector<uint8_t>& data);
};

/**
 * @brief Encrypted message envelope
 * 
 * Supports multiple cipher modes:
 * - Version 0x02: AES-256-CBC with HMAC (Encrypt-then-MAC)
 * - Version 0x03: AES-256-GCM (AEAD, recommended)
 * 
 * Format v2 (CBC): [Version (1)] [Sequence (8)] [IV (16)] [Ciphertext] [HMAC (32)]
 * Format v3 (GCM): [Version (1)] [Sequence (8)] [Nonce (12)] [Ciphertext+Tag]
 */
struct EncryptedMessage {
    static constexpr uint8_t VERSION_CBC_HMAC = 0x02;  // CBC + HMAC
    static constexpr uint8_t VERSION_GCM = 0x03;       // GCM AEAD (recommended)
    static constexpr uint8_t CURRENT_VERSION = VERSION_GCM;
    
    uint8_t version = CURRENT_VERSION;
    uint64_t sequence = 0;                    // Replay protection
    std::vector<uint8_t> iv;                  // 16 bytes for CBC, 12 bytes for GCM
    std::vector<uint8_t> ciphertext;          // For GCM: includes 16-byte auth tag at end
    std::vector<uint8_t> hmac;                // Only for CBC mode
    
    // Serialize to wire format
    std::vector<uint8_t> serialize() const;
    
    // Deserialize from wire format
    static EncryptedMessage deserialize(const std::vector<uint8_t>& data);
    
    // Get data covered by HMAC/AAD (version || sequence || IV/nonce)
    std::vector<uint8_t> getAuthenticatedData() const;
    
    // Check if using AEAD mode
    bool isAead() const { return version >= VERSION_GCM; }
};

} // namespace SentinelFS
