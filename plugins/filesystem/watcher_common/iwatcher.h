#ifndef SFS_IWATCHER_H
#define SFS_IWATCHER_H

#include <string>
#include <functional>
#include <cstdint>

namespace sfs {
namespace plugins {

/**
 * @brief File system event types
 */
enum class FsEventType {
    CREATED,        // New file/directory created
    MODIFIED,       // File content or metadata changed
    DELETED,        // File/directory deleted
    RENAMED_OLD,    // File renamed (old name)
    RENAMED_NEW,    // File renamed (new name)
    UNKNOWN         // Unknown or error
};

/**
 * @brief File system event
 * 
 * Represents a filesystem change detected by the watcher.
 */
struct FsEvent {
    FsEventType type;
    std::string path;           // Affected file/directory path
    std::string extra_info;     // Additional info (e.g., new name for rename)
    uint64_t timestamp;         // Event timestamp (ms since epoch)
    bool is_directory;          // True if event is for a directory
    
    FsEvent()
        : type(FsEventType::UNKNOWN)
        , timestamp(0)
        , is_directory(false) {}
    
    FsEvent(FsEventType t, const std::string& p)
        : type(t)
        , path(p)
        , timestamp(current_time_ms())
        , is_directory(false) {}
    
    static uint64_t current_time_ms();
};

/**
 * @brief IWatcher - File system watcher interface
 * 
 * Abstract interface for platform-specific file watchers.
 * Plugins implement this interface to provide:
 * - Linux: inotify
 * - macOS: FSEvents
 * - Windows: ReadDirectoryChangesW
 * 
 * This interface is used by Core through EventBus.
 */
class IWatcher {
public:
    virtual ~IWatcher() = default;
    
    /**
     * @brief Event callback
     * 
     * Set this callback to receive filesystem events.
     * The watcher will call this function for each detected change.
     */
    std::function<void(const FsEvent&)> on_event;
    
    /**
     * @brief Start watching a directory
     * 
     * Begins monitoring the specified directory for changes.
     * Recursive monitoring depends on platform capabilities.
     * 
     * @param path Directory path to watch
     * @return true if started successfully
     */
    virtual bool start(const std::string& path) = 0;
    
    /**
     * @brief Stop watching
     * 
     * Stops monitoring and releases resources.
     */
    virtual void stop() = 0;
    
    /**
     * @brief Check if watcher is running
     * 
     * @return true if currently watching
     */
    virtual bool is_running() const = 0;
    
    /**
     * @brief Get watched path
     * 
     * @return Currently watched directory path
     */
    virtual std::string get_watched_path() const = 0;
};

/**
 * @brief Convert FsEventType to string
 */
inline const char* event_type_to_string(FsEventType type) {
    switch (type) {
        case FsEventType::CREATED: return "CREATED";
        case FsEventType::MODIFIED: return "MODIFIED";
        case FsEventType::DELETED: return "DELETED";
        case FsEventType::RENAMED_OLD: return "RENAMED_OLD";
        case FsEventType::RENAMED_NEW: return "RENAMED_NEW";
        case FsEventType::UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

} // namespace plugins
} // namespace sfs

#endif // SFS_IWATCHER_H
