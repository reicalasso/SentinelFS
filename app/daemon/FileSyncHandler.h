#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

namespace SentinelFS {

class INetworkAPI;
class IStorageAPI;

/**
 * @brief Handles file synchronization events
 * 
 * Responsible for broadcasting file changes to peers.
 * Works with EventHandlers to prevent sync loops.
 */
class FileSyncHandler {
public:
    FileSyncHandler(INetworkAPI* network, IStorageAPI* storage, const std::string& watchDir);

    /**
     * @brief Handle file modification event
     * @param fullPath Absolute path to modified file
     * 
     * Broadcasts UPDATE_AVAILABLE to all connected peers.
     * Respects syncEnabled flag.
     */
    void handleFileModified(const std::string& fullPath);

    /**
     * @brief Enable/disable sync operations
     * @param enabled true to enable, false to disable
     */
    void setSyncEnabled(bool enabled) { syncEnabled_ = enabled; }
    
    /**
     * @brief Check if sync is currently enabled
     */
    bool isSyncEnabled() const { return syncEnabled_; }

private:
    INetworkAPI* network_;
    IStorageAPI* storage_;
    std::string watchDirectory_;
    std::atomic<bool> syncEnabled_{true};
};

} // namespace SentinelFS
