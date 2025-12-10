#include "Compression.h"
#include <zlib.h>
#include <cstring>

namespace sfs {

std::vector<uint8_t> Compression::compress(
    const std::vector<uint8_t>& data,
    CompressionLevel level
) {
    if (data.empty() || data.size() < MIN_COMPRESS_SIZE) {
        return {}; // Too small to compress
    }
    
    // Estimate compressed size (worst case is slightly larger than input)
    uLongf compressedSize = compressBound(static_cast<uLong>(data.size()));
    
    // Allocate buffer with header space
    // Header: 4 bytes magic + 4 bytes original size
    std::vector<uint8_t> compressed(8 + compressedSize);
    
    // Write header
    uint32_t magic = COMPRESS_MAGIC;
    uint32_t originalSize = static_cast<uint32_t>(data.size());
    std::memcpy(compressed.data(), &magic, 4);
    std::memcpy(compressed.data() + 4, &originalSize, 4);
    
    // Compress
    int zlibLevel = static_cast<int>(level);
    int result = compress2(
        compressed.data() + 8,
        &compressedSize,
        data.data(),
        static_cast<uLong>(data.size()),
        zlibLevel
    );
    
    if (result != Z_OK) {
        return {}; // Compression failed
    }
    
    // Resize to actual compressed size + header
    compressed.resize(8 + compressedSize);
    
    // Only return compressed data if it's actually smaller
    if (compressed.size() >= data.size()) {
        return {}; // Compression not beneficial
    }
    
    return compressed;
}

std::vector<uint8_t> Compression::decompress(const std::vector<uint8_t>& data) {
    if (data.size() < 8) {
        return {}; // Too small to have header
    }
    
    // Read header
    uint32_t magic;
    uint32_t originalSize;
    std::memcpy(&magic, data.data(), 4);
    std::memcpy(&originalSize, data.data() + 4, 4);
    
    // Verify magic
    if (magic != COMPRESS_MAGIC) {
        return {}; // Not compressed data
    }
    
    // Sanity check on size (max 1GB)
    if (originalSize > 1024 * 1024 * 1024) {
        return {}; // Suspicious size
    }
    
    // Allocate output buffer
    std::vector<uint8_t> decompressed(originalSize);
    uLongf destLen = originalSize;
    
    // Decompress
    int result = uncompress(
        decompressed.data(),
        &destLen,
        data.data() + 8,
        static_cast<uLong>(data.size() - 8)
    );
    
    if (result != Z_OK || destLen != originalSize) {
        return {}; // Decompression failed
    }
    
    return decompressed;
}

bool Compression::isCompressible(const std::vector<uint8_t>& data) {
    if (data.size() < MIN_COMPRESS_SIZE) {
        return false;
    }
    
    // Quick entropy check - count unique bytes in sample
    // High entropy (random/encrypted) data doesn't compress well
    constexpr size_t sampleSize = 256;
    size_t checkSize = std::min(data.size(), sampleSize);
    
    bool seen[256] = {false};
    int uniqueBytes = 0;
    
    for (size_t i = 0; i < checkSize; ++i) {
        if (!seen[data[i]]) {
            seen[data[i]] = true;
            ++uniqueBytes;
        }
    }
    
    // If more than 90% of possible byte values are used, likely not compressible
    double entropyRatio = static_cast<double>(uniqueBytes) / 256.0;
    return entropyRatio < 0.9;
}

double Compression::compressionRatio(size_t originalSize, size_t compressedSize) {
    if (originalSize == 0) return 1.0;
    return static_cast<double>(compressedSize) / static_cast<double>(originalSize);
}

} // namespace sfs
