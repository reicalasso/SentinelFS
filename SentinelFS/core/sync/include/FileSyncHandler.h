#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <map>
#include <mutex>
#include <chrono>

namespace SentinelFS {

class INetworkAPI;
class IStorageAPI;

/**
 * @brief Handles file synchronization events
 * 
 * Responsible for broadcasting file changes to peers.
 * Works with EventHandlers to prevent sync loops.
 * Supports ignore patterns for filtering files.
 */
class FileSyncHandler {
public:
    FileSyncHandler(INetworkAPI* network, IStorageAPI* storage, const std::string& watchDir);

    /**
     * @brief Handle file modification event
     * @param fullPath Absolute path to modified file
     * 
     * Broadcasts UPDATE_AVAILABLE to all connected peers.
     * Respects syncEnabled flag and ignore patterns.
     */
    void handleFileModified(const std::string& fullPath);

    /**
     * @brief Handle file deletion event
     * @param fullPath Absolute path to deleted file
     * 
     * Removes file from database and broadcasts DELETE_FILE to peers.
     */
    void handleFileDeleted(const std::string& fullPath);

    /**
     * @brief Perform scan of a directory
     * @param path Optional directory path to scan. If empty, scans default watch directory.
     * 
     * Scans all files in the directory and adds/updates
     * their metadata in the database. Respects ignore patterns.
     */
    void scanDirectory(const std::string& path = "");

    /**
     * @brief Enable/disable sync operations
     * @param enabled true to enable, false to disable
     */
    void setSyncEnabled(bool enabled) { syncEnabled_ = enabled; }
    
    /**
     * @brief Check if sync is currently enabled
     */
    bool isSyncEnabled() const { return syncEnabled_; }
    
    /**
     * @brief Reload ignore patterns from database
     */
    void loadIgnorePatterns();
    
    /**
     * @brief Update file metadata in database (always runs, even when paused)
     * @param fullPath Absolute path to modified file
     * @return true if successfully updated database
     */
    bool updateFileInDatabase(const std::string& fullPath);
    
    /**
     * @brief Broadcast file update to peers (only when sync enabled)
     * @param fullPath Absolute path to modified file
     * @param hash Pre-computed hash (optional, avoids re-computation)
     * @param size Pre-computed size (optional, avoids re-computation)
     */
    void broadcastUpdate(const std::string& fullPath, const std::string& hash = "", long long size = -1);

    /**
     * @brief Broadcast file deletion to peers (only when sync enabled)
     * @param fullPath Absolute path to deleted file
     */
    void broadcastDelete(const std::string& fullPath);

    /**
     * @brief Broadcast all local files to a specific peer
     * @param peerId The peer to send updates to
     * 
     * Used when a new peer connects to send them our file list.
     */
    void broadcastAllFilesToPeer(const std::string& peerId);

    /**
     * @brief Result of file metadata computation
     */
    struct FileMetadataResult {
        std::string hash;
        long long size;
        long long timestamp;
        bool valid;
    };
    
    /**
     * @brief Compute file metadata (hash, size, timestamp)
     * @param fullPath Absolute path to file
     * @return FileMetadataResult with computed values
     */
    FileMetadataResult computeFileMetadata(const std::string& fullPath);

    /**
     * @brief Default ignore patterns (VCS, build artifacts, IDE files, temp files)
     * 
     * These patterns are always applied. Users can add additional patterns
     * via the database which are checked after these defaults.
     */
    static const std::vector<std::string> DEFAULT_IGNORE_PATTERNS;

private:
    std::string calculateFileHash(const std::string& path);
    bool shouldIgnore(const std::string& path);
    
    // Hash cache entry
    struct HashCacheEntry {
        std::string hash;
        std::chrono::system_clock::time_point mtime;
        std::chrono::steady_clock::time_point cachedAt;
    };
    
    // Hash cache for recently computed hashes (avoids recomputing for same file)
    mutable std::map<std::string, HashCacheEntry> hashCache_;
    mutable std::mutex hashCacheMutex_;
    static constexpr size_t MAX_CACHE_SIZE = 1000;  // Limit cache size
    static constexpr std::chrono::minutes CACHE_TTL{5};  // Cache TTL
    
    // Get cached hash or compute new one
    std::string getCachedHash(const std::string& path);

    INetworkAPI* network_;
    IStorageAPI* storage_;
    std::string watchDirectory_;
    std::atomic<bool> syncEnabled_{true};
    std::vector<std::string> ignorePatterns_;
};

} // namespace SentinelFS
