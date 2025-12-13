#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace SentinelFS {

struct BlockSignature;
struct DeltaInstruction;

/**
 * @brief Serialization utilities for delta sync protocol
 */
class DeltaSerialization {
public:
    /**
     * @brief Serialize block signatures
     */
    static std::vector<uint8_t> serializeSignature(const std::vector<BlockSignature>& sigs);

    /**
     * @brief Deserialize block signatures
     */
    static std::vector<BlockSignature> deserializeSignature(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize delta instructions
     */
    static std::vector<uint8_t> serializeDelta(const std::vector<DeltaInstruction>& deltas, size_t blockSize);

    /**
     * @brief Deserialize delta instructions
     */
    static std::vector<DeltaInstruction> deserializeDelta(const std::vector<uint8_t>& data, size_t& outBlockSize);
};

} // namespace SentinelFS
