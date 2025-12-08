#pragma once

/**
 * @file IronRootPlugin.h
 * @brief IronRoot - Advanced Filesystem Monitoring Plugin
 * 
 * Features:
 * - fanotify support (process-aware monitoring)
 * - Event debouncing (coalesce rapid changes)
 * - Atomic write detection (temp file → rename pattern)
 * - Extended attributes (xattr) support
 * - Batch processing for bulk operations
 * - Symlink handling
 * - File locking for concurrent access
 * - Memory-mapped I/O for large files
 */

#include "IFileAPI.h"
#include "IWatcher.h"
#include "FileOps.h"
#include "Debouncer.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <queue>
#include <condition_variable>

namespace SentinelFS {

class EventBus;

namespace IronRoot {

/**
 * @brief Extended watch event with process info
 */
struct IronWatchEvent {
    WatchEventType type;
    std::string path;
    std::string oldPath;  // For renames
    bool isDirectory{false};
    
    // Process information (fanotify)
    pid_t pid{0};
    std::string processName;
    std::string processPath;
    
    // Timing
    std::chrono::steady_clock::time_point timestamp;
    
    // Batch info
    uint32_t batchId{0};
    bool isBatchStart{false};
    bool isBatchEnd{false};
};

/**
 * @brief Watch configuration
 */
struct WatchConfig {
    bool recursive{true};
    bool followSymlinks{false};
    bool trackXattrs{true};
    bool useFanotify{true};          // Use fanotify if available (requires CAP_SYS_ADMIN)
    bool fallbackToInotify{true};    // Fallback to inotify if fanotify fails
    DebounceConfig debounce;
    std::vector<std::string> ignorePatterns;
    std::vector<std::string> includePatterns;  // If set, only watch these
};

/**
 * @brief Callback for watch events
 */
using IronWatchCallback = std::function<void(const IronWatchEvent&)>;

/**
 * @brief Callback for batch events
 */
using BatchCallback = std::function<void(const std::vector<IronWatchEvent>&)>;

} // namespace IronRoot

/**
 * @brief IronRoot main plugin class
 */
class IronRootPlugin : public IFileAPI {
public:
    IronRootPlugin();
    ~IronRootPlugin() override;
    
    // IPlugin interface
    bool initialize(EventBus* eventBus) override;
    void shutdown() override;
    std::string getName() const override { return "IronRoot"; }
    std::string getVersion() const override { return "1.0.0"; }
    
    // IFileAPI interface
    std::vector<uint8_t> readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override;
    void startWatching(const std::string& path) override;
    void stopWatching(const std::string& path) override;
    
    // Extended API
    
    /**
     * @brief Read file with memory mapping (efficient for large files)
     */
    std::vector<uint8_t> readFileMapped(const std::string& path);
    
    /**
     * @brief Write file atomically (write to temp, then rename)
     */
    bool writeFileAtomic(const std::string& path, const std::vector<uint8_t>& data);
    
    /**
     * @brief Get extended file information
     */
    IronRoot::FileInfo getFileInfo(const std::string& path);
    
    /**
     * @brief Get/set extended attributes
     */
    std::map<std::string, std::string> getXattrs(const std::string& path);
    bool setXattr(const std::string& path, const std::string& name, const std::string& value);
    bool removeXattr(const std::string& path, const std::string& name);
    
    /**
     * @brief File locking
     */
    bool lockFile(const std::string& path, bool exclusive = true);
    bool unlockFile(const std::string& path);
    bool isFileLocked(const std::string& path) const;
    
    /**
     * @brief Configure watching
     */
    void setWatchConfig(const IronRoot::WatchConfig& config);
    IronRoot::WatchConfig getWatchConfig() const;
    
    /**
     * @brief Set callbacks
     */
    void setWatchCallback(IronRoot::IronWatchCallback callback);
    void setBatchCallback(IronRoot::BatchCallback callback);
    
    /**
     * @brief Check capabilities
     */
    bool hasFanotifySupport() const;
    bool hasXattrSupport() const;
    
    /**
     * @brief Statistics
     */
    struct Stats {
        uint64_t eventsProcessed{0};
        uint64_t eventsDebounced{0};
        uint64_t atomicWritesDetected{0};
        uint64_t batchesProcessed{0};
        uint64_t filesWatched{0};
        uint64_t dirsWatched{0};
        uint64_t bytesRead{0};
        uint64_t bytesWritten{0};
    };
    Stats getStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SentinelFS
