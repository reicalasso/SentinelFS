#ifndef SFS_WATCHER_LINUX_H
#define SFS_WATCHER_LINUX_H

#include "../watcher_common/iwatcher.h"
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <sys/inotify.h>

namespace sfs {
namespace plugins {

/**
 * @brief Linux file system watcher using inotify
 * 
 * Implements IWatcher using Linux inotify API.
 * Monitors a directory recursively for file system events.
 */
class WatcherLinux : public IWatcher {
public:
    WatcherLinux();
    ~WatcherLinux() override;
    
    bool start(const std::string& path) override;
    void stop() override;
    bool is_running() const override;
    std::string get_watched_path() const override;

private:
    int inotify_fd_;                        // inotify file descriptor
    std::atomic<bool> running_;             // Running state
    std::string watch_path_;                // Root path being watched
    std::thread watch_thread_;              // Event processing thread
    std::map<int, std::string> watch_descriptors_;  // wd -> path mapping
    
    /**
     * @brief Watch thread main loop
     */
    void watch_loop();
    
    /**
     * @brief Add watch for directory
     */
    bool add_watch(const std::string& path);
    
    /**
     * @brief Remove watch for directory
     */
    void remove_watch(int wd);
    
    /**
     * @brief Process inotify event
     */
    void process_event(const ::inotify_event* event, const std::string& base_path);
    
    /**
     * @brief Recursively add watches for all subdirectories
     */
    void add_recursive_watches(const std::string& path);
};

} // namespace plugins
} // namespace sfs

#endif // SFS_WATCHER_LINUX_H
