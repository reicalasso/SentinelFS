#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include "../../core/include/EventBus.h"
#include "../../core/include/INetworkAPI.h"
#include "../../core/include/IStorageAPI.h"
#include "../../core/include/IFileAPI.h"

namespace SentinelFS {

/**
 * @brief Event handler coordinator
 * 
 * Manages all EventBus subscriptions for daemon operations:
 * - PEER_DISCOVERED: Add discovered peers to storage, auto-connect
 * - FILE_MODIFIED: Broadcast update notifications to peers
 * - DATA_RECEIVED: Process delta sync protocol messages
 * - ANOMALY_DETECTED: Stop sync for safety
 */
class EventHandlers {
public:
    /**
     * @brief Constructor
     * @param eventBus Event bus to subscribe to
     * @param network Network plugin for peer communication
     * @param storage Storage plugin for peer/file management
     * @param filesystem Filesystem plugin for file operations
     * @param watchDirectory Base directory being watched
     */
    EventHandlers(EventBus& eventBus,
                  INetworkAPI* network,
                  IStorageAPI* storage,
                  IFileAPI* filesystem,
                  const std::string& watchDirectory);
    
    ~EventHandlers();
    
    /**
     * @brief Setup all event subscriptions
     */
    void setupHandlers();
    
    /**
     * @brief Enable/disable sync operations
     * 
     * When disabled, FILE_MODIFIED events are ignored
     */
    void setSyncEnabled(bool enabled);
    
    /**
     * @brief Check if sync is currently enabled
     */
    bool isSyncEnabled() const { return syncEnabled_; }
    
private:
    // Event handler implementations
    void handlePeerDiscovered(const std::any& data);
    void handleFileModified(const std::any& data);
    void handleDataReceived(const std::any& data);
    void handleAnomalyDetected(const std::any& data);
    
    // Delta sync protocol handlers
    void handleUpdateAvailable(const std::string& peerId, const std::vector<uint8_t>& rawData);
    void handleDeltaRequest(const std::string& peerId, const std::vector<uint8_t>& rawData);
    void handleDeltaData(const std::string& peerId, const std::vector<uint8_t>& rawData);
    
    EventBus& eventBus_;
    INetworkAPI* network_;
    IStorageAPI* storage_;
    IFileAPI* filesystem_;
    std::string watchDirectory_;
    
    std::atomic<bool> syncEnabled_{true};
    
    // Ignore list to prevent sync loops
    std::mutex ignoreMutex_;
    std::map<std::string, std::chrono::steady_clock::time_point> ignoreList_;
};

} // namespace SentinelFS
