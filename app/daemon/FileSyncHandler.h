#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace SentinelFS {

class INetworkAPI;
class IStorageAPI;

/**
 * @brief Handles file synchronization events
 */
class FileSyncHandler {
public:
    FileSyncHandler(INetworkAPI* network, IStorageAPI* storage, const std::string& watchDir);

    /**
     * @brief Handle file modification event
     */
    void handleFileModified(const std::string& fullPath);

    /**
     * @brief Enable/disable sync
     */
    void setSyncEnabled(bool enabled) { syncEnabled_ = enabled; }

    /**
     * @brief Check if path should be ignored (recently patched)
     */
    bool shouldIgnore(const std::string& filename);

    /**
     * @brief Mark file as recently patched
     */
    void markAsPatched(const std::string& filename);

private:
    INetworkAPI* network_;
    IStorageAPI* storage_;
    std::string watchDirectory_;
    bool syncEnabled_ = true;
};

} // namespace SentinelFS
