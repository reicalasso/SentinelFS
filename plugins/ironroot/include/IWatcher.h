#pragma once

/**
 * @file IWatcher.h
 * @brief Abstract watcher interface for IronRoot
 */

#include <string>
#include <functional>
#include <optional>

namespace SentinelFS {
namespace IronRoot {

/**
 * @brief Watch event types
 */
enum class WatchEventType {
    Create,
    Modify,
    Delete,
    Rename,
    AttribChange,  // Permissions, xattrs, etc.
    Open,          // File opened (fanotify only)
    Close,         // File closed (fanotify only)
    Access         // File accessed (fanotify only)
};

/**
 * @brief Basic watch event
 */
struct WatchEvent {
    WatchEventType type;
    std::string path;
    std::optional<std::string> oldPath;  // For renames
    bool isDirectory{false};
    
    // Process info (fanotify)
    pid_t pid{0};
    std::string processName;
};

/**
 * @brief Watch callback type
 */
using WatchCallback = std::function<void(const WatchEvent&)>;

/**
 * @brief Abstract watcher interface
 */
class IWatcher {
public:
    virtual ~IWatcher() = default;
    
    /**
     * @brief Initialize the watcher
     */
    virtual bool initialize(WatchCallback callback) = 0;
    
    /**
     * @brief Shutdown the watcher
     */
    virtual void shutdown() = 0;
    
    /**
     * @brief Add a path to watch
     */
    virtual bool addWatch(const std::string& path) = 0;
    
    /**
     * @brief Remove a watched path
     */
    virtual bool removeWatch(const std::string& path) = 0;
    
    /**
     * @brief Add recursive watch
     */
    virtual void addWatchRecursive(const std::string& path) = 0;
    
    /**
     * @brief Check if path is being watched
     */
    virtual bool isWatching(const std::string& path) const = 0;
    
    /**
     * @brief Get number of watched paths
     */
    virtual size_t getWatchCount() const = 0;
    
    /**
     * @brief Get watcher name
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Check if watcher supports process info
     */
    virtual bool supportsProcessInfo() const { return false; }
};

} // namespace IronRoot
} // namespace SentinelFS
