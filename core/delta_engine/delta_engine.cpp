#include "delta_engine.h"
#include <openssl/sha.h>
#include <unordered_map>
#include <cstring>
#include <algorithm>

namespace sfs {
namespace core {

// Adler-32 constants
constexpr uint32_t ADLER_MOD = 65521;

// Implementation details
class RsyncDeltaEngine::Impl {
public:
    // Hash table for fast block lookup by weak checksum
    std::unordered_multimap<WeakChecksum, const BlockSignature*> signature_map_;
    
    void build_signature_map(const std::vector<BlockSignature>& signatures) {
        signature_map_.clear();
        for (const auto& sig : signatures) {
            signature_map_.insert({sig.weak, &sig});
        }
    }
    
    const BlockSignature* find_matching_block(
        WeakChecksum weak,
        const StrongHash& strong
    ) {
        auto range = signature_map_.equal_range(weak);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second->strong == strong) {
                return it->second;
            }
        }
        return nullptr;
    }
};

RsyncDeltaEngine::RsyncDeltaEngine()
    : impl_(std::make_unique<Impl>()) {
}

RsyncDeltaEngine::~RsyncDeltaEngine() = default;

std::vector<BlockSignature> RsyncDeltaEngine::generate_signatures(
    const uint8_t* data,
    size_t data_size,
    size_t block_size
) {
    std::vector<BlockSignature> signatures;
    
    size_t num_blocks = (data_size + block_size - 1) / block_size;
    signatures.reserve(num_blocks);
    
    for (size_t i = 0; i < num_blocks; ++i) {
        size_t offset = i * block_size;
        size_t length = std::min(block_size, data_size - offset);
        
        BlockSignature sig;
        sig.index = i;
        sig.offset = offset;
        sig.weak = compute_weak_checksum(data + offset, length);
        sig.strong = compute_strong_hash(data + offset, length);
        
        signatures.push_back(sig);
    }
    
    return signatures;
}

DeltaResult RsyncDeltaEngine::compute_delta(
    const uint8_t* new_data,
    size_t new_data_size,
    const std::vector<BlockSignature>& base_signatures,
    size_t block_size
) {
    DeltaResult result;
    result.original_size = new_data_size;
    result.delta_size = 0;
    result.matched_blocks = 0;
    result.literal_bytes = 0;
    
    // Build hash table for fast lookup
    impl_->build_signature_map(base_signatures);
    
    size_t pos = 0;
    std::vector<uint8_t> literal_buffer;
    
    while (pos < new_data_size) {
        size_t remaining = new_data_size - pos;
        size_t window_size = std::min(block_size, remaining);
        
        // Compute checksums for current window
        WeakChecksum weak = compute_weak_checksum(new_data + pos, window_size);
        StrongHash strong = compute_strong_hash(new_data + pos, window_size);
        
        // Try to find matching block
        const BlockSignature* match = impl_->find_matching_block(weak, strong);
        
        if (match) {
            // Found matching block - flush any pending literals
            if (!literal_buffer.empty()) {
                DeltaOp literal_op;
                literal_op.type = DeltaOpType::LITERAL;
                literal_op.literal_data = literal_buffer;
                result.operations.push_back(literal_op);
                result.delta_size += literal_buffer.size();
                result.literal_bytes += literal_buffer.size();
                literal_buffer.clear();
            }
            
            // Add reference operation
            DeltaOp ref_op;
            ref_op.type = DeltaOpType::REFERENCE;
            ref_op.block_index = match->index;
            ref_op.block_offset = match->offset;
            ref_op.block_length = window_size;
            result.operations.push_back(ref_op);
            result.delta_size += 16; // Approximate overhead for reference
            result.matched_blocks++;
            
            // Skip the matched block
            pos += window_size;
        } else {
            // No match - add byte to literal buffer
            literal_buffer.push_back(new_data[pos]);
            pos++;
        }
    }
    
    // Flush remaining literals
    if (!literal_buffer.empty()) {
        DeltaOp literal_op;
        literal_op.type = DeltaOpType::LITERAL;
        literal_op.literal_data = literal_buffer;
        result.operations.push_back(literal_op);
        result.delta_size += literal_buffer.size();
        result.literal_bytes += literal_buffer.size();
    }
    
    return result;
}

bool RsyncDeltaEngine::apply_delta(
    const uint8_t* base_data,
    size_t base_data_size,
    const DeltaResult& delta,
    std::vector<uint8_t>& output
) {
    output.clear();
    output.reserve(delta.original_size);
    
    for (const auto& op : delta.operations) {
        if (op.type == DeltaOpType::LITERAL) {
            // Copy literal data
            output.insert(output.end(), op.literal_data.begin(), op.literal_data.end());
        } else if (op.type == DeltaOpType::REFERENCE) {
            // Copy block from base data
            if (op.block_offset + op.block_length > base_data_size) {
                return false; // Invalid reference
            }
            output.insert(
                output.end(),
                base_data + op.block_offset,
                base_data + op.block_offset + op.block_length
            );
        }
    }
    
    return true;
}

WeakChecksum RsyncDeltaEngine::compute_weak_checksum(
    const uint8_t* data,
    size_t length
) {
    // Adler-32 algorithm
    uint32_t a = 1;
    uint32_t b = 0;
    
    for (size_t i = 0; i < length; ++i) {
        a = (a + data[i]) % ADLER_MOD;
        b = (b + a) % ADLER_MOD;
    }
    
    return (b << 16) | a;
}

WeakChecksum RsyncDeltaEngine::update_rolling_checksum(
    WeakChecksum old_checksum,
    uint8_t old_byte,
    uint8_t new_byte,
    size_t block_size
) {
    // Extract a and b from old checksum
    uint32_t a = old_checksum & 0xFFFF;
    uint32_t b = (old_checksum >> 16) & 0xFFFF;
    
    // Update a: remove old byte, add new byte
    a = (a - old_byte + new_byte) % ADLER_MOD;
    
    // Update b: remove old contribution, add new contribution
    b = (b - (block_size * old_byte) + a - 1) % ADLER_MOD;
    
    return (b << 16) | a;
}

StrongHash RsyncDeltaEngine::compute_strong_hash(
    const uint8_t* data,
    size_t length
) {
    StrongHash hash;
    SHA256(data, length, hash.data);
    return hash;
}

} // namespace core
} // namespace sfs
