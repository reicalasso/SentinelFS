#pragma once

/**
 * @file SyncPipeline.h
 * @brief 7-Stage Sync Pipeline Implementation
 * 
 * Orchestrates the complete sync flow:
 * 1. Discovery - Find peers (delegated to NetFalcon)
 * 2. Handshake - mTLS + capability exchange
 * 3. Meta Transfer - File metadata exchange
 * 4. Hash Scan - Signature calculation
 * 5. Delta Request - Delta computation
 * 6. Block Stream - Data transfer
 * 7. ACK/Finalize - Integrity verification
 */

#include "SyncProtocol.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <memory>
#include <atomic>

namespace SentinelFS {

class INetworkAPI;
class IStorageAPI;
class IFileAPI;

namespace Sync {

/**
 * @brief Callback types for pipeline events
 */
using TransferProgressCallback = std::function<void(const std::string& transferId, 
                                                     uint64_t bytesTransferred, 
                                                     uint64_t totalBytes)>;
using TransferCompleteCallback = std::function<void(const std::string& transferId, 
                                                     bool success, 
                                                     const std::string& error)>;
using StateChangeCallback = std::function<void(const std::string& peerId, 
                                                TransferState oldState, 
                                                TransferState newState)>;

/**
 * @brief Peer session state after handshake
 */
struct PeerSession {
    std::string peerId;
    Capability negotiatedCaps{Capability::NONE};
    uint32_t agreedBlockSize{BLOCK_SIZE};
    uint32_t agreedChunkSize{CHUNK_SIZE};
    bool authenticated{false};
    std::chrono::steady_clock::time_point lastActivity;
    uint32_t nextSequence{0};
    
    // Replay protection - track received sequences
    std::vector<uint32_t> receivedSequences;
    static constexpr size_t MAX_SEQUENCE_HISTORY = 1000;
};

/**
 * @brief Main sync pipeline orchestrator
 */
class SyncPipeline {
public:
    SyncPipeline(INetworkAPI* network, IStorageAPI* storage, 
                 IFileAPI* filesystem, const std::string& watchDir);
    ~SyncPipeline();
    
    // ========================================================================
    // Stage 2: Handshake
    // ========================================================================
    
    /**
     * @brief Initiate handshake with a peer
     * @param peerId Target peer ID
     * @return true if handshake initiated successfully
     */
    bool initiateHandshake(const std::string& peerId);
    
    /**
     * @brief Handle incoming handshake init
     */
    void handleHandshakeInit(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle handshake response
     */
    void handleHandshakeResponse(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle handshake complete
     */
    void handleHandshakeComplete(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Check if peer session is established
     */
    bool isPeerAuthenticated(const std::string& peerId) const;
    
    // ========================================================================
    // Stage 3: Meta Transfer
    // ========================================================================
    
    /**
     * @brief Send file metadata to peer
     * @param peerId Target peer
     * @param localPath Local file path
     * @return Transfer ID for tracking
     */
    std::string sendFileMeta(const std::string& peerId, const std::string& localPath);
    
    /**
     * @brief Handle incoming file metadata
     */
    void handleFileMeta(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle file metadata acknowledgment
     */
    void handleFileMetaAck(const std::string& peerId, const std::vector<uint8_t>& data);
    
    // ========================================================================
    // Stage 4: Hash Scan (Signature Exchange)
    // ========================================================================
    
    /**
     * @brief Request signature from peer for delta sync
     */
    void requestSignature(const std::string& peerId, const std::string& relativePath);
    
    /**
     * @brief Handle signature request - compute and send our signature
     */
    void handleSignatureRequest(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle signature response - proceed to delta computation
     */
    void handleSignatureResponse(const std::string& peerId, const std::vector<uint8_t>& data);
    
    // ========================================================================
    // Stage 5: Delta Request
    // ========================================================================
    
    /**
     * @brief Compute and send delta based on received signature
     */
    void computeAndSendDelta(const std::string& peerId, const std::string& relativePath,
                             const std::vector<uint8_t>& peerSignature);
    
    /**
     * @brief Handle incoming delta data
     */
    void handleDeltaResponse(const std::string& peerId, const std::vector<uint8_t>& data);
    
    // ========================================================================
    // Stage 6: Block Stream
    // ========================================================================
    
    /**
     * @brief Stream file blocks to peer
     */
    void streamBlocks(const std::string& peerId, const std::string& relativePath,
                      const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle incoming block data
     */
    void handleBlockData(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle block acknowledgment (for flow control)
     */
    void handleBlockAck(const std::string& peerId, const std::vector<uint8_t>& data);
    
    // ========================================================================
    // Stage 7: ACK/Finalize
    // ========================================================================
    
    /**
     * @brief Send transfer complete with final hash
     */
    void sendTransferComplete(const std::string& peerId, const std::string& transferId);
    
    /**
     * @brief Handle transfer complete - verify integrity
     */
    void handleTransferComplete(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle transfer acknowledgment
     */
    void handleTransferAck(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle integrity failure - trigger retry
     */
    void handleIntegrityFail(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Handle transfer abort - cleanup transfer
     */
    void handleTransferAbort(const std::string& peerId, const std::vector<uint8_t>& data);
    
    // ========================================================================
    // High-Level API
    // ========================================================================
    
    /**
     * @brief Start a complete file sync to peer (all 7 stages)
     * @param peerId Target peer
     * @param localPath Local file path
     * @return Transfer ID for tracking
     */
    std::string syncFileToPeer(const std::string& peerId, const std::string& localPath);
    
    /**
     * @brief Broadcast file update to all authenticated peers
     */
    void broadcastFileUpdate(const std::string& localPath);
    
    /**
     * @brief Get transfer context by ID
     */
    const TransferContext* getTransfer(const std::string& transferId) const;
    
    /**
     * @brief Get all active transfers
     */
    std::vector<TransferContext> getActiveTransfers() const;
    
    /**
     * @brief Abort a transfer
     */
    void abortTransfer(const std::string& transferId);
    
    // ========================================================================
    // Callbacks
    // ========================================================================
    
    void setProgressCallback(TransferProgressCallback cb) { progressCallback_ = std::move(cb); }
    void setCompleteCallback(TransferCompleteCallback cb) { completeCallback_ = std::move(cb); }
    void setStateChangeCallback(StateChangeCallback cb) { stateChangeCallback_ = std::move(cb); }
    
    /**
     * @brief Set callback for marking files as patched (to prevent sync loops)
     */
    void setMarkAsPatchedCallback(std::function<void(const std::string&)> cb) {
        markAsPatchedCallback_ = std::move(cb);
    }
    
    // ========================================================================
    // Message Routing
    // ========================================================================
    
    /**
     * @brief Route incoming message to appropriate handler
     * @param peerId Source peer
     * @param data Raw message data
     */
    void handleMessage(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Get local capabilities
     */
    Capability getLocalCapabilities() const { return localCapabilities_; }
    
private:
    // Helper methods
    std::string generateTransferId();
    std::string getRelativePath(const std::string& absolutePath) const;
    std::string getAbsolutePath(const std::string& relativePath) const;
    std::vector<uint8_t> calculateFileHash(const std::string& path) const;
    void updateTransferState(const std::string& transferId, TransferState newState);
    void cleanupStaleTransfers();
    uint32_t getNextSequence(const std::string& peerId);
    bool validateSequence(const std::string& peerId, uint32_t sequence);
    
    // Send helpers
    bool sendMessage(const std::string& peerId, const std::vector<uint8_t>& data);
    
    // Dependencies
    INetworkAPI* network_;
    IStorageAPI* storage_;
    IFileAPI* filesystem_;
    std::string watchDirectory_;
    
    // Local peer info
    std::string localPeerId_;
    Capability localCapabilities_{DEFAULT_CAPABILITIES};
    
    // Session management
    mutable std::mutex sessionMutex_;
    std::map<std::string, PeerSession> peerSessions_;
    
    // Transfer management
    mutable std::mutex transferMutex_;
    std::map<std::string, TransferContext> activeTransfers_;
    std::map<std::string, std::string> pathToTransfer_;  // relativePath -> transferId
    
    // Pending data for chunked transfers
    struct PendingChunks {
        std::string relativePath;
        uint32_t totalChunks{0};
        uint32_t receivedChunks{0};
        std::vector<std::vector<uint8_t>> chunks;
        std::chrono::steady_clock::time_point lastActivity;
    };
    mutable std::mutex chunkMutex_;
    std::map<std::string, PendingChunks> pendingChunks_;  // peerId|path -> chunks
    
    // Callbacks
    TransferProgressCallback progressCallback_;
    TransferCompleteCallback completeCallback_;
    StateChangeCallback stateChangeCallback_;
    std::function<void(const std::string&)> markAsPatchedCallback_;
    
    // Cleanup
    std::atomic<bool> running_{true};
    static constexpr int TRANSFER_TIMEOUT_SECONDS = 300;
    static constexpr int MAX_RETRIES = 3;
};

} // namespace Sync
} // namespace SentinelFS
