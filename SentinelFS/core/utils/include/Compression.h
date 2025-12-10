#pragma once

/**
 * @file Compression.h
 * @brief Data compression utilities for SentinelFS
 * 
 * Provides compression/decompression for delta transfers.
 * Uses zlib for compatibility (available on most systems).
 */

#include <vector>
#include <cstdint>
#include <string>

namespace sfs {

/**
 * @brief Compression level presets
 */
enum class CompressionLevel {
    None = 0,
    Fast = 1,
    Default = 6,
    Best = 9
};

/**
 * @brief Compression utilities for network transfer optimization
 * 
 * Usage:
 * @code
 * std::vector<uint8_t> data = ...;
 * auto compressed = Compression::compress(data);
 * auto decompressed = Compression::decompress(compressed);
 * @endcode
 */
class Compression {
public:
    /**
     * @brief Compress data using zlib deflate
     * @param data Input data to compress
     * @param level Compression level (0-9)
     * @return Compressed data, or empty vector on failure
     */
    static std::vector<uint8_t> compress(
        const std::vector<uint8_t>& data,
        CompressionLevel level = CompressionLevel::Default
    );
    
    /**
     * @brief Decompress zlib-compressed data
     * @param data Compressed data
     * @return Decompressed data, or empty vector on failure
     */
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);
    
    /**
     * @brief Check if compression would be beneficial
     * @param data Input data
     * @return true if data is likely compressible
     */
    static bool isCompressible(const std::vector<uint8_t>& data);
    
    /**
     * @brief Get compression ratio
     * @param originalSize Original data size
     * @param compressedSize Compressed data size
     * @return Compression ratio (e.g., 0.5 = 50% of original)
     */
    static double compressionRatio(size_t originalSize, size_t compressedSize);
    
    /**
     * @brief Minimum size for compression to be worthwhile
     */
    static constexpr size_t MIN_COMPRESS_SIZE = 256;
    
    /**
     * @brief Magic header for compressed data
     */
    static constexpr uint32_t COMPRESS_MAGIC = 0x5A4C4942; // "ZLIB"
};

} // namespace sfs
