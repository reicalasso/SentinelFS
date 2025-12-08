#pragma once

/**
 * @file FanotifyWatcher.h
 * @brief fanotify-based filesystem watcher (Linux)
 * 
 * fanotify advantages over inotify:
 * - Process information (PID, process name)
 * - Permission events (can block operations)
 * - Filesystem-wide monitoring
 * - Better for security applications
 * 
 * Requires CAP_SYS_ADMIN capability or root
 */

#include "IWatcher.h"
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>

namespace SentinelFS {
namespace IronRoot {

class FanotifyWatcher : public IWatcher {
public:
    FanotifyWatcher();
    ~FanotifyWatcher() override;
    
    bool initialize(WatchCallback callback) override;
    void shutdown() override;
    bool addWatch(const std::string& path) override;
    bool removeWatch(const std::string& path) override;
    void addWatchRecursive(const std::string& path) override;
    bool isWatching(const std::string& path) const override;
    size_t getWatchCount() const override;
    std::string getName() const override { return "fanotify"; }
    bool supportsProcessInfo() const override { return true; }
    
    /**
     * @brief Check if fanotify is available on this system
     */
    static bool isAvailable();

private:
    void monitorLoop();
    std::string getProcessName(pid_t pid);
    std::string getProcessPath(pid_t pid);
    
    int fanotifyFd_{-1};
    std::atomic<bool> running_{false};
    std::thread watcherThread_;
    WatchCallback callback_;
    
    mutable std::mutex watchMutex_;
    std::set<std::string> watchedPaths_;
    
    // Process name cache
    mutable std::mutex procCacheMutex_;
    std::map<pid_t, std::string> processNameCache_;
};

} // namespace IronRoot
} // namespace SentinelFS
