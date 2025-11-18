#pragma once

#include "delta_types.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace sfs {
namespace core {

/**
 * IDeltaEngine - Rsync-style delta compression interface
 * 
 * Implements rolling checksum algorithm to efficiently compute
 * the differences between two files and synchronize only changed blocks.
 */
class IDeltaEngine {
public:
    virtual ~IDeltaEngine() = default;
    
    /**
     * Generate block signatures for a file (base file on receiver side)
     */
    virtual std::vector<BlockSignature> generate_signatures(
        const uint8_t* data,
        size_t data_size,
        size_t block_size = DEFAULT_BLOCK_SIZE
    ) = 0;
    
    /**
     * Compute delta between new file and base file signatures
     */
    virtual DeltaResult compute_delta(
        const uint8_t* new_data,
        size_t new_data_size,
        const std::vector<BlockSignature>& base_signatures,
        size_t block_size = DEFAULT_BLOCK_SIZE
    ) = 0;
    
    /**
     * Apply delta to reconstruct new file from base file
     */
    virtual bool apply_delta(
        const uint8_t* base_data,
        size_t base_data_size,
        const DeltaResult& delta,
        std::vector<uint8_t>& output
    ) = 0;
    
    /**
     * Compute weak checksum (Adler-32) for a block
     */
    virtual WeakChecksum compute_weak_checksum(
        const uint8_t* data,
        size_t length
    ) = 0;
    
    /**
     * Update rolling checksum (sliding window)
     */
    virtual WeakChecksum update_rolling_checksum(
        WeakChecksum old_checksum,
        uint8_t old_byte,
        uint8_t new_byte,
        size_t block_size
    ) = 0;
    
    /**
     * Compute strong hash (SHA-256) for a block
     */
    virtual StrongHash compute_strong_hash(
        const uint8_t* data,
        size_t length
    ) = 0;
};

/**
 * RsyncDeltaEngine - Reference implementation of IDeltaEngine
 */
class RsyncDeltaEngine : public IDeltaEngine {
public:
    RsyncDeltaEngine();
    ~RsyncDeltaEngine() override;
    
    std::vector<BlockSignature> generate_signatures(
        const uint8_t* data,
        size_t data_size,
        size_t block_size = DEFAULT_BLOCK_SIZE
    ) override;
    
    DeltaResult compute_delta(
        const uint8_t* new_data,
        size_t new_data_size,
        const std::vector<BlockSignature>& base_signatures,
        size_t block_size = DEFAULT_BLOCK_SIZE
    ) override;
    
    bool apply_delta(
        const uint8_t* base_data,
        size_t base_data_size,
        const DeltaResult& delta,
        std::vector<uint8_t>& output
    ) override;
    
    WeakChecksum compute_weak_checksum(
        const uint8_t* data,
        size_t length
    ) override;
    
    WeakChecksum update_rolling_checksum(
        WeakChecksum old_checksum,
        uint8_t old_byte,
        uint8_t new_byte,
        size_t block_size
    ) override;
    
    StrongHash compute_strong_hash(
        const uint8_t* data,
        size_t length
    ) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace core
} // namespace sfs
