#pragma once

/**
 * @file SyncProtocol.h
 * @brief Wire-level sync protocol definitions
 * 
 * SentinelFS 7-Stage Sync Pipeline:
 * 
 * ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
 * │ DISCOVER│───▶│HANDSHAKE│───▶│  META   │───▶│  HASH   │
 * │  Peer   │    │  mTLS   │    │Transfer │    │  Scan   │
 * └─────────┘    └─────────┘    └─────────┘    └─────────┘
 *                                                   │
 *      ┌────────────────────────────────────────────┘
 *      ▼
 * ┌─────────┐    ┌─────────┐    ┌─────────┐
 * │  DELTA  │───▶│  BLOCK  │───▶│FINALIZE │
 * │ Request │    │ Stream  │    │  ACK    │
 * └─────────┘    └─────────┘    └─────────┘
 */

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace SentinelFS {
namespace Sync {

// ============================================================================
// Protocol Constants
// ============================================================================

constexpr uint32_t PROTOCOL_MAGIC = 0x53454E54;  // "SENT" in ASCII
constexpr uint16_t PROTOCOL_VERSION = 0x0002;
constexpr size_t BLOCK_SIZE = 4096;              // 4KB blocks for delta sync
constexpr size_t CHUNK_SIZE = 64 * 1024;         // 64KB network chunks
constexpr size_t LARGE_FILE_THRESHOLD = 100 * 1024 * 1024;  // 100MB streaming threshold

// ============================================================================
// Message Types (Wire Protocol)
// ============================================================================

enum class MessageType : uint8_t {
    // Stage 1: Discovery (handled by NetFalcon)
    DISCOVER_REQUEST    = 0x01,
    DISCOVER_RESPONSE   = 0x02,
    
    // Stage 2: Handshake
    HANDSHAKE_INIT      = 0x10,
    HANDSHAKE_RESPONSE  = 0x11,
    HANDSHAKE_COMPLETE  = 0x12,
    HANDSHAKE_REJECT    = 0x13,
    
    // Stage 3: Meta Transfer
    FILE_META           = 0x20,
    FILE_META_ACK       = 0x21,
    FILE_LIST_REQUEST   = 0x22,
    FILE_LIST_RESPONSE  = 0x23,
    
    // Stage 4: Hash Scan (Signature Exchange)
    SIGNATURE_REQUEST   = 0x30,
    SIGNATURE_RESPONSE  = 0x31,
    
    // Stage 5: Delta Request
    DELTA_REQUEST       = 0x40,
    DELTA_RESPONSE      = 0x41,
    
    // Stage 6: Block Stream
    BLOCK_DATA          = 0x50,
    BLOCK_ACK           = 0x51,
    FULL_FILE_REQUEST   = 0x52,
    FULL_FILE_DATA      = 0x53,
    
    // Stage 7: Finalize
    TRANSFER_COMPLETE   = 0x60,
    TRANSFER_ACK        = 0x61,
    TRANSFER_ABORT      = 0x62,
    INTEGRITY_FAIL      = 0x63,
    
    // Control Messages
    PING                = 0xF0,
    PONG                = 0xF1,
    ERROR               = 0xFF
};

// ============================================================================
// Capability Flags (for Handshake)
// ============================================================================

enum class Capability : uint32_t {
    NONE                = 0,
    DELTA_SYNC          = 1 << 0,   // Supports delta synchronization
    COMPRESSION_ZSTD    = 1 << 1,   // Supports Zstandard compression
    COMPRESSION_LZ4     = 1 << 2,   // Supports LZ4 compression
    ENCRYPTION_AES_GCM  = 1 << 3,   // Supports AES-256-GCM
    ENCRYPTION_CHACHA   = 1 << 4,   // Supports ChaCha20-Poly1305
    STREAMING           = 1 << 5,   // Supports large file streaming
    RESUME              = 1 << 6,   // Supports transfer resume
    INTEGRITY_SHA256    = 1 << 7,   // Uses SHA-256 for integrity
    INTEGRITY_BLAKE3    = 1 << 8,   // Uses BLAKE3 for integrity
};

constexpr Capability operator|(Capability a, Capability b) {
    return static_cast<Capability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr Capability operator&(Capability a, Capability b) {
    return static_cast<Capability>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasCapability(Capability caps, Capability flag) {
    return (static_cast<uint32_t>(caps) & static_cast<uint32_t>(flag)) != 0;
}

// Default capabilities for SentinelFS
constexpr Capability DEFAULT_CAPABILITIES = 
    Capability::DELTA_SYNC | 
    Capability::COMPRESSION_ZSTD |
    Capability::ENCRYPTION_AES_GCM |
    Capability::STREAMING |
    Capability::RESUME |
    Capability::INTEGRITY_SHA256;

// ============================================================================
// Wire Protocol Structures
// ============================================================================

#pragma pack(push, 1)

/**
 * @brief Common message header (8 bytes)
 */
struct MessageHeader {
    uint32_t magic;         // PROTOCOL_MAGIC
    uint16_t version;       // PROTOCOL_VERSION
    uint8_t  type;          // MessageType
    uint8_t  flags;         // Message-specific flags
    uint32_t payloadSize;   // Size of payload following header
    uint32_t sequence;      // Sequence number for ordering/replay protection
};

/**
 * @brief Handshake init message
 */
struct HandshakeInit {
    MessageHeader header;
    uint8_t  peerId[32];        // UUID of initiating peer
    uint32_t capabilities;      // Capability flags
    uint32_t maxBlockSize;      // Maximum block size supported
    uint32_t maxChunkSize;      // Maximum chunk size for network transfer
    uint8_t  sessionCodeHash[32]; // SHA-256 of session code (for verification)
};

/**
 * @brief Handshake response message
 */
struct HandshakeResponse {
    MessageHeader header;
    uint8_t  peerId[32];        // UUID of responding peer
    uint32_t capabilities;      // Negotiated capabilities (intersection)
    uint32_t agreedBlockSize;   // Agreed block size
    uint32_t agreedChunkSize;   // Agreed chunk size
    uint8_t  challenge[32];     // Random challenge for authentication
};

/**
 * @brief File metadata structure
 */
struct FileMeta {
    MessageHeader header;
    uint64_t fileSize;          // File size in bytes
    uint64_t modTime;           // Modification time (Unix timestamp)
    uint32_t permissions;       // File permissions
    uint8_t  fileType;          // 0=regular, 1=symlink, 2=directory
    uint8_t  hashType;          // 0=SHA-256, 1=BLAKE3
    uint8_t  fileHash[32];      // Hash of file content
    uint16_t pathLength;        // Length of relative path
    // Followed by: char path[pathLength]
};

/**
 * @brief Signature request/response for delta sync
 */
struct SignatureMessage {
    MessageHeader header;
    uint16_t pathLength;        // Length of relative path
    uint32_t blockCount;        // Number of signature blocks
    // Followed by: char path[pathLength]
    // Followed by: BlockSignature[blockCount]
};

/**
 * @brief Block signature for delta sync (36 bytes per block)
 */
struct BlockSignatureWire {
    uint32_t rollingHash;       // Adler-32 rolling hash
    uint8_t  strongHash[32];    // SHA-256 strong hash
};

/**
 * @brief Delta instruction (variable size)
 */
struct DeltaInstruction {
    uint8_t  type;              // 0=COPY, 1=LITERAL
    uint32_t offset;            // For COPY: source offset; For LITERAL: data length
    uint32_t length;            // For COPY: block count; For LITERAL: unused
    // For LITERAL: followed by uint8_t data[offset]
};

/**
 * @brief Block data message for streaming
 */
struct BlockData {
    MessageHeader header;
    uint16_t pathLength;        // Length of relative path
    uint32_t chunkIndex;        // Current chunk index
    uint32_t totalChunks;       // Total number of chunks
    uint32_t dataSize;          // Size of data in this chunk
    // Followed by: char path[pathLength]
    // Followed by: uint8_t data[dataSize]
};

/**
 * @brief Transfer complete message
 */
struct TransferComplete {
    MessageHeader header;
    uint16_t pathLength;        // Length of relative path
    uint8_t  finalHash[32];     // SHA-256 of complete file
    uint64_t bytesTransferred;  // Total bytes transferred
    uint32_t durationMs;        // Transfer duration in milliseconds
    // Followed by: char path[pathLength]
};

/**
 * @brief Transfer acknowledgment
 */
struct TransferAck {
    MessageHeader header;
    uint16_t pathLength;        // Length of relative path
    uint8_t  verified;          // 1=hash verified, 0=hash mismatch
    uint8_t  computedHash[32];  // Hash computed by receiver
    // Followed by: char path[pathLength]
};

#pragma pack(pop)

// ============================================================================
// High-Level Transfer State
// ============================================================================

enum class TransferState {
    IDLE,
    HANDSHAKING,
    SENDING_META,
    AWAITING_META_ACK,
    COMPUTING_SIGNATURE,
    SENDING_SIGNATURE,
    COMPUTING_DELTA,
    STREAMING_BLOCKS,
    AWAITING_ACK,
    COMPLETE,
    FAILED,
    ABORTED
};

/**
 * @brief Tracks state of an ongoing file transfer
 */
struct TransferContext {
    std::string transferId;
    std::string peerId;
    std::string relativePath;
    std::string localPath;
    
    TransferState state{TransferState::IDLE};
    
    // File info
    uint64_t fileSize{0};
    uint64_t bytesTransferred{0};
    std::vector<uint8_t> fileHash;
    
    // Delta sync
    bool useDelta{false};
    uint32_t deltaInstructions{0};
    uint64_t savedBytes{0};  // Bytes saved by delta sync
    
    // Timing
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastActivity;
    
    // Chunking
    uint32_t currentChunk{0};
    uint32_t totalChunks{0};
    
    // Error handling
    int retryCount{0};
    std::string lastError;
};

// ============================================================================
// Protocol Helper Functions
// ============================================================================

/**
 * @brief Create a message header
 */
inline MessageHeader createHeader(MessageType type, uint32_t payloadSize, uint32_t sequence) {
    MessageHeader header{};
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = static_cast<uint8_t>(type);
    header.flags = 0;
    header.payloadSize = payloadSize;
    header.sequence = sequence;
    return header;
}

/**
 * @brief Validate a message header
 */
inline bool validateHeader(const MessageHeader& header) {
    return header.magic == PROTOCOL_MAGIC && 
           header.version <= PROTOCOL_VERSION;
}

/**
 * @brief Get human-readable message type name
 */
inline const char* messageTypeName(MessageType type) {
    switch (type) {
        case MessageType::DISCOVER_REQUEST:   return "DISCOVER_REQUEST";
        case MessageType::DISCOVER_RESPONSE:  return "DISCOVER_RESPONSE";
        case MessageType::HANDSHAKE_INIT:     return "HANDSHAKE_INIT";
        case MessageType::HANDSHAKE_RESPONSE: return "HANDSHAKE_RESPONSE";
        case MessageType::HANDSHAKE_COMPLETE: return "HANDSHAKE_COMPLETE";
        case MessageType::HANDSHAKE_REJECT:   return "HANDSHAKE_REJECT";
        case MessageType::FILE_META:          return "FILE_META";
        case MessageType::FILE_META_ACK:      return "FILE_META_ACK";
        case MessageType::FILE_LIST_REQUEST:  return "FILE_LIST_REQUEST";
        case MessageType::FILE_LIST_RESPONSE: return "FILE_LIST_RESPONSE";
        case MessageType::SIGNATURE_REQUEST:  return "SIGNATURE_REQUEST";
        case MessageType::SIGNATURE_RESPONSE: return "SIGNATURE_RESPONSE";
        case MessageType::DELTA_REQUEST:      return "DELTA_REQUEST";
        case MessageType::DELTA_RESPONSE:     return "DELTA_RESPONSE";
        case MessageType::BLOCK_DATA:         return "BLOCK_DATA";
        case MessageType::BLOCK_ACK:          return "BLOCK_ACK";
        case MessageType::FULL_FILE_REQUEST:  return "FULL_FILE_REQUEST";
        case MessageType::FULL_FILE_DATA:     return "FULL_FILE_DATA";
        case MessageType::TRANSFER_COMPLETE:  return "TRANSFER_COMPLETE";
        case MessageType::TRANSFER_ACK:       return "TRANSFER_ACK";
        case MessageType::TRANSFER_ABORT:     return "TRANSFER_ABORT";
        case MessageType::INTEGRITY_FAIL:     return "INTEGRITY_FAIL";
        case MessageType::PING:               return "PING";
        case MessageType::PONG:               return "PONG";
        case MessageType::ERROR:              return "ERROR";
        default:                              return "UNKNOWN";
    }
}

/**
 * @brief Get human-readable transfer state name
 */
inline const char* transferStateName(TransferState state) {
    switch (state) {
        case TransferState::IDLE:               return "IDLE";
        case TransferState::HANDSHAKING:        return "HANDSHAKING";
        case TransferState::SENDING_META:       return "SENDING_META";
        case TransferState::AWAITING_META_ACK:  return "AWAITING_META_ACK";
        case TransferState::COMPUTING_SIGNATURE:return "COMPUTING_SIGNATURE";
        case TransferState::SENDING_SIGNATURE:  return "SENDING_SIGNATURE";
        case TransferState::COMPUTING_DELTA:    return "COMPUTING_DELTA";
        case TransferState::STREAMING_BLOCKS:   return "STREAMING_BLOCKS";
        case TransferState::AWAITING_ACK:       return "AWAITING_ACK";
        case TransferState::COMPLETE:           return "COMPLETE";
        case TransferState::FAILED:             return "FAILED";
        case TransferState::ABORTED:            return "ABORTED";
        default:                                return "UNKNOWN";
    }
}

} // namespace Sync
} // namespace SentinelFS
