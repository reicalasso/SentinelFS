#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace SentinelFS {

/**
 * @brief Handles file hashing operations
 */
class FileHasher {
public:
    /**
     * @brief Calculate SHA256 hash of file
     */
    static std::string calculateSHA256(const std::string& filePath);

    /**
     * @brief Calculate SHA256 hash of byte array
     */
    static std::string calculateSHA256(const uint8_t* data, size_t length);
};

} // namespace SentinelFS
