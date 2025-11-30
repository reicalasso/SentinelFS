#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>

namespace SentinelFS {

class INetworkAPI;
class IStorageAPI;
class IFileAPI;

/**
 * @brief Handles delta sync protocol messages
 * 
 * Implements the 3-way delta sync protocol:
 * 1. UPDATE_AVAILABLE: Peer notifies us of file change
 * 2. REQUEST_DELTA: We send our file signature
 * 3. DELTA_DATA: Peer sends delta instructions
 * 
 * Uses DeltaEngine for efficient bandwidth usage.
 */
class DeltaSyncProtocolHandler {
public:
    DeltaSyncProtocolHandler(INetworkAPI* network, IStorageAPI* storage, 
                            IFileAPI* filesystem, const std::string& watchDir);
    
    ~DeltaSyncProtocolHandler();

    /**
     * @brief Handle UPDATE_AVAILABLE message from peer
     * @param peerId Peer that sent the message
     * @param data Raw message data
     * 
     * Calculates local file signature and requests delta.
     */
    void handleUpdateAvailable(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Handle REQUEST_DELTA message from peer
     * @param peerId Peer that sent the message
     * @param data Raw message data containing peer's signature
     * 
     * Calculates delta and sends DELTA_DATA response.
     */
    void handleDeltaRequest(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Handle DELTA_DATA message from peer
     * @param peerId Peer that sent the message
     * @param data Raw message data containing delta instructions
     * 
     * Applies delta to local file and notifies ignore list.
     */
    void handleDeltaData(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Handle REQUEST_FILE message from peer (full file request)
     * @param peerId Peer that sent the message
     * @param data Raw message data containing file path
     */
    void handleFileRequest(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Handle FILE_DATA message from peer (full file transfer)
     * @param peerId Peer that sent the message
     * @param data Raw message data containing file content
     */
    void handleFileData(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Handle DELETE_FILE message from peer
     * @param peerId Peer that sent the message
     * @param data Raw message data containing file path
     */
    void handleDeleteFile(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Set callback for marking files as patched
     */
    void setMarkAsPatchedCallback(std::function<void(const std::string&)> callback) {
        markAsPatchedCallback_ = callback;
    }

private:
    void cleanupStaleChunks();
    void startCleanupThread();
    void stopCleanupThread();
    
    INetworkAPI* network_;
    IStorageAPI* storage_;
    IFileAPI* filesystem_;
    std::string watchDirectory_;
    std::function<void(const std::string&)> markAsPatchedCallback_;

    struct PendingDeltaChunks {
        std::uint32_t totalChunks{0};
        std::uint32_t receivedChunks{0};
        std::vector<std::vector<uint8_t>> chunks;
        std::chrono::steady_clock::time_point lastActivity;  // For timeout cleanup
    };

    mutable std::mutex pendingMutex_;
    std::unordered_map<std::string, PendingDeltaChunks> pendingDeltas_;
    
    // Cleanup thread for stale pending chunks
    std::thread cleanupThread_;
    std::atomic<bool> cleanupRunning_{false};
    static constexpr int CHUNK_TIMEOUT_SECONDS = 300;  // 5 minutes
    static constexpr int CLEANUP_INTERVAL_SECONDS = 60;  // Check every minute
};

} // namespace SentinelFS
