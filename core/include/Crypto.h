#ifndef SENTINEL_CRYPTO_H
#define SENTINEL_CRYPTO_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <array>

namespace sentinel {

/**
 * @brief Cryptographic utilities for secure data transfer
 * 
 * Provides AES-256-CBC encryption/decryption with PKCS7 padding
 * and secure key exchange using Elliptic Curve Diffie-Hellman (ECDH)
 */
class Crypto {
public:
    static constexpr size_t KEY_SIZE = 32;      // 256 bits
    static constexpr size_t IV_SIZE = 16;       // 128 bits
    static constexpr size_t BLOCK_SIZE = 16;    // AES block size
    
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
     * @brief Derive a shared secret from session code
     * @param sessionCode 6-character session code
     * @param salt Random salt for key derivation
     * @param iterations PBKDF2 iteration count (default: 100000)
     * @return 32-byte derived key
     */
    static std::vector<uint8_t> deriveKeyFromSessionCode(
        const std::string& sessionCode,
        const std::vector<uint8_t>& salt,
        int iterations = 100000
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
 * Format: [IV (16 bytes)] [Ciphertext (variable)] [HMAC (32 bytes)]
 */
struct EncryptedMessage {
    std::vector<uint8_t> iv;
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> hmac;
    
    // Serialize to wire format
    std::vector<uint8_t> serialize() const;
    
    // Deserialize from wire format
    static EncryptedMessage deserialize(const std::vector<uint8_t>& data);
};

} // namespace sentinel

#endif // SENTINEL_CRYPTO_H
