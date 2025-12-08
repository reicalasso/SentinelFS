#pragma once

/**
 * @file InotifyWatcher.h
 * @brief inotify-based filesystem watcher (Linux fallback)
 */

#include "IWatcher.h"
#include <map>
#include <mutex>
#include <thread>
#include <atomic>

namespace SentinelFS {
namespace IronRoot {

class InotifyWatcher : public IWatcher {
public:
    InotifyWatcher();
    ~InotifyWatcher() override;
    
    bool initialize(WatchCallback callback) override;
    void shutdown() override;
    bool addWatch(const std::string& path) override;
    bool removeWatch(const std::string& path) override;
    void addWatchRecursive(const std::string& path) override;
    bool isWatching(const std::string& path) const override;
    size_t getWatchCount() const override;
    std::string getName() const override { return "inotify"; }
    
private:
    void monitorLoop();
    std::string getWatchPath(int wd) const;
    
    int inotifyFd_{-1};
    std::atomic<bool> running_{false};
    std::thread watcherThread_;
    WatchCallback callback_;
    
    mutable std::mutex watchMutex_;
    std::map<int, std::string> watchDescriptors_;  // wd -> path
    std::map<std::string, int> pathToWd_;          // path -> wd
};

} // namespace IronRoot
} // namespace SentinelFS
