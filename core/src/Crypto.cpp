#include "Crypto.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace sentinel {

std::vector<uint8_t> Crypto::generateKey() {
    std::vector<uint8_t> key(KEY_SIZE);
    if (RAND_bytes(key.data(), KEY_SIZE) != 1) {
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
    if (key.size() != KEY_SIZE) {
        throw std::runtime_error("Invalid key size");
    }
    if (iv.size() != IV_SIZE) {
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
    if (key.size() != KEY_SIZE) {
        throw std::runtime_error("Invalid key size");
    }
    if (iv.size() != IV_SIZE) {
        throw std::runtime_error("Invalid IV size");
    }
    if (ciphertext.size() % BLOCK_SIZE != 0) {
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
        throw std::runtime_error("Key derivation failed");
    }
    
    return key;
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

std::vector<uint8_t> EncryptedMessage::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(iv.size() + ciphertext.size() + hmac.size());
    
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    result.insert(result.end(), hmac.begin(), hmac.end());
    
    return result;
}

EncryptedMessage EncryptedMessage::deserialize(const std::vector<uint8_t>& data) {
    const size_t min_size = Crypto::IV_SIZE + Crypto::BLOCK_SIZE + 32; // IV + at least 1 block + HMAC
    
    if (data.size() < min_size) {
        throw std::runtime_error("Invalid encrypted message size");
    }
    
    EncryptedMessage msg;
    
    // Extract IV (first 16 bytes)
    msg.iv.assign(data.begin(), data.begin() + Crypto::IV_SIZE);
    
    // Extract HMAC (last 32 bytes)
    msg.hmac.assign(data.end() - 32, data.end());
    
    // Extract ciphertext (everything in between)
    msg.ciphertext.assign(
        data.begin() + Crypto::IV_SIZE,
        data.end() - 32
    );
    
    return msg;
}

} // namespace sentinel
