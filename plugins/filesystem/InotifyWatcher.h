#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <functional>

namespace SentinelFS {

/**
 * @brief Monitors filesystem changes using inotify
 */
class InotifyWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& eventType, const std::string& path)>;

    InotifyWatcher();
    ~InotifyWatcher();

    // Prevent copying
    InotifyWatcher(const InotifyWatcher&) = delete;
    InotifyWatcher& operator=(const InotifyWatcher&) = delete;

    /**
     * @brief Initialize inotify and start monitoring thread
     */
    bool initialize(ChangeCallback callback);

    /**
     * @brief Stop monitoring and cleanup
     */
    void shutdown();

    /**
     * @brief Add directory to watch list
     */
    bool addWatch(const std::string& path);

    /**
     * @brief Remove directory from watch list
     */
    bool removeWatch(const std::string& path);

private:
    int inotifyFd_ = -1;
    std::thread watcherThread_;
    std::atomic<bool> running_{false};
    std::map<int, std::string> watchDescriptors_;
    ChangeCallback callback_;

    /**
     * @brief Main monitoring loop
     */
    void monitorLoop();
};

} // namespace SentinelFS
