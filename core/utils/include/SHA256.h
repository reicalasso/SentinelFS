#pragma once

#include <string>
#include <vector>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

namespace SentinelFS {

/**
 * @brief Simple SHA256 hashing utility
 */
class SHA256 {
public:
    /**
     * @brief Hash a string to hex-encoded SHA256
     */
    static std::string hash(const std::string& input) {
        return hashBytes(std::vector<uint8_t>(input.begin(), input.end()));
    }
    
    /**
     * @brief Hash binary data to hex-encoded SHA256
     */
    static std::string hashBytes(const std::vector<uint8_t>& data) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";
        
        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        
        if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        
        unsigned int hashLen;
        if (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        
        EVP_MD_CTX_free(ctx);
        
        // Convert to hex string
        std::stringstream ss;
        for (unsigned int i = 0; i < hashLen; ++i) {
            ss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
        }
        
        return ss.str();
    }
};

} // namespace SentinelFS
