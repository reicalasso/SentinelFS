#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

namespace sfs {
namespace core {

// Block size for delta operations (default: 4KB)
constexpr size_t DEFAULT_BLOCK_SIZE = 4096;

// Weak checksum result (Adler-32)
using WeakChecksum = uint32_t;

// Strong hash result (SHA-256)
struct StrongHash {
    uint8_t data[32];
    
    bool operator==(const StrongHash& other) const {
        return std::memcmp(data, other.data, 32) == 0;
    }
    
    bool operator!=(const StrongHash& other) const {
        return !(*this == other);
    }
    
    std::string to_hex() const;
};

// Block signature (weak + strong checksum)
struct BlockSignature {
    size_t index;           // Block index in original file
    size_t offset;          // Byte offset in original file
    WeakChecksum weak;      // Fast rolling checksum
    StrongHash strong;      // Strong cryptographic hash
};

// Delta operation type
enum class DeltaOpType {
    LITERAL,    // Copy literal data
    REFERENCE   // Reference to existing block
};

// Single delta operation
struct DeltaOp {
    DeltaOpType type;
    
    // For LITERAL: the actual data
    std::vector<uint8_t> literal_data;
    
    // For REFERENCE: block index in base file
    size_t block_index;
    size_t block_offset;
    size_t block_length;
};

// Complete delta result
struct DeltaResult {
    std::vector<DeltaOp> operations;
    size_t original_size;
    size_t delta_size;
    
    // Statistics
    size_t matched_blocks;
    size_t literal_bytes;
    
    double compression_ratio() const {
        if (original_size == 0) return 0.0;
        return 1.0 - (static_cast<double>(delta_size) / original_size);
    }
};

} // namespace core
} // namespace sfs
