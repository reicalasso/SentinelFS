#include "InotifyWatcher.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>

namespace SentinelFS {

InotifyWatcher::InotifyWatcher() = default;

InotifyWatcher::~InotifyWatcher() {
    shutdown();
}

bool InotifyWatcher::initialize(ChangeCallback callback) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::INFO, "Initializing inotify watcher", "InotifyWatcher");
    callback_ = callback;
    
    // Use non-blocking inotify
    inotifyFd_ = inotify_init1(IN_NONBLOCK);
    if (inotifyFd_ < 0) {
        logger.log(LogLevel::ERROR, "Failed to initialize inotify: " + std::string(strerror(errno)), "InotifyWatcher");
        metrics.incrementSyncErrors();
        return false;
    }

    running_ = true;
    watcherThread_ = std::thread(&InotifyWatcher::monitorLoop, this);
    logger.log(LogLevel::INFO, "Inotify watcher initialized successfully", "InotifyWatcher");
    return true;
}

void InotifyWatcher::shutdown() {
    auto& logger = Logger::instance();
    
    logger.log(LogLevel::INFO, "Shutting down inotify watcher", "InotifyWatcher");
    running_ = false;
    
    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
    
    if (inotifyFd_ >= 0) {
        close(inotifyFd_);
        inotifyFd_ = -1;
    }
    
    logger.log(LogLevel::INFO, "Inotify watcher shut down", "InotifyWatcher");
}

bool InotifyWatcher::addWatch(const std::string& path) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    if (inotifyFd_ < 0) {
        logger.log(LogLevel::ERROR, "Cannot add watch - inotify not initialized", "InotifyWatcher");
        return false;
    }
    
    // Check if already watching this path
    {
        std::lock_guard<std::mutex> lock(watchMutex_);
        for (const auto& pair : watchDescriptors_) {
            if (pair.second == path) {
                logger.log(LogLevel::DEBUG, "Already watching: " + path, "InotifyWatcher");
                return true;
            }
        }
    }
    
    logger.log(LogLevel::DEBUG, "Adding watch for: " + path, "InotifyWatcher");
    
    int wd = inotify_add_watch(inotifyFd_, path.c_str(), 
                                IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM | IN_CLOSE_WRITE);
    if (wd < 0) {
        logger.log(LogLevel::ERROR, "Failed to add watch for " + path + ": " + std::string(strerror(errno)), "InotifyWatcher");
        metrics.incrementSyncErrors();
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(watchMutex_);
        watchDescriptors_[wd] = path;
    }
    
    logger.log(LogLevel::INFO, "Now watching directory: " + path, "InotifyWatcher");
    metrics.incrementFilesWatched();
    return true;
}

bool InotifyWatcher::removeWatch(const std::string& path) {
    auto& logger = Logger::instance();
    
    logger.log(LogLevel::DEBUG, "Removing watch for: " + path, "InotifyWatcher");
    
    std::lock_guard<std::mutex> lock(watchMutex_);
    for (auto it = watchDescriptors_.begin(); it != watchDescriptors_.end(); ++it) {
        if (it->second == path) {
            if (inotify_rm_watch(inotifyFd_, it->first) < 0) {
                logger.log(LogLevel::WARN, "Failed to remove watch for " + path + ": " + std::string(strerror(errno)), "InotifyWatcher");
            } else {
                logger.log(LogLevel::INFO, "Removed watch for: " + path, "InotifyWatcher");
            }
            watchDescriptors_.erase(it);
            return true;
        }
    }
    logger.log(LogLevel::WARN, "Watch not found for path: " + path, "InotifyWatcher");
    return false;
}

void InotifyWatcher::addWatchRecursive(const std::string& path) {
    auto& logger = Logger::instance();
    namespace fs = std::filesystem;
    
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
        return;
    }
    
    // Add watch for this directory
    addWatch(path);
    
    // Add watch for all subdirectories
    for (auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ++it) {
        if (ec) {
            logger.log(LogLevel::WARN, "Error iterating directory: " + ec.message(), "InotifyWatcher");
            break;
        }
        if (it->is_directory(ec)) {
            addWatch(it->path().string());
        }
    }
}

std::string InotifyWatcher::getWatchPath(int wd) const {
    std::lock_guard<std::mutex> lock(watchMutex_);
    auto it = watchDescriptors_.find(wd);
    if (it != watchDescriptors_.end()) {
        return it->second;
    }
    return "";
}

void InotifyWatcher::monitorLoop() {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Monitor loop started", "InotifyWatcher");
    
    const int EVENT_SIZE = sizeof(struct inotify_event);
    const int BUF_LEN = 4096 * (EVENT_SIZE + 256);
    char buffer[BUF_LEN];

    while (running_) {
        // Use select with timeout to allow clean shutdown
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(inotifyFd_, &fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms timeout
        
        int ret = select(inotifyFd_ + 1, &fds, nullptr, nullptr, &timeout);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            if (running_) {
                logger.log(LogLevel::ERROR, "Select error: " + std::string(strerror(errno)), "InotifyWatcher");
            }
            break;
        }
        
        if (ret == 0) {
            // Timeout, check if we should continue
            continue;
        }
        
        int length = read(inotifyFd_, buffer, BUF_LEN);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // No data available (non-blocking)
            }
            if (running_) {
                logger.log(LogLevel::ERROR, "Inotify read error: " + std::string(strerror(errno)), "InotifyWatcher");
                metrics.incrementSyncErrors();
            }
            break;
        }

        if (length == 0) {
            continue;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            if (event->len > 0) {
                std::string dirPath = getWatchPath(event->wd);
                if (!dirPath.empty()) {
                    std::string filename = event->name;
                    std::string fullPath = dirPath + "/" + filename;
                    
                    WatchEventType type;
                    bool hasType = false;
                    bool isDir = (event->mask & IN_ISDIR) != 0;

                    if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {
                        type = WatchEventType::Create;
                        hasType = true;
                        
                        // If a new directory was created, automatically add watch for it
                        if (isDir) {
                            logger.log(LogLevel::INFO, "New directory detected, adding watch: " + fullPath, "InotifyWatcher");
                            addWatchRecursive(fullPath);
                        }
                    } else if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) {
                        type = WatchEventType::Delete;
                        hasType = true;
                        metrics.incrementFilesDeleted();
                    } else if (event->mask & IN_MODIFY) {
                        // Skip IN_MODIFY for directories
                        if (!isDir) {
                            type = WatchEventType::Modify;
                            hasType = true;
                            metrics.incrementFilesModified();
                        }
                    } else if (event->mask & IN_CLOSE_WRITE) {
                        // File was closed after writing - reliable indicator of file change
                        if (!isDir) {
                            type = WatchEventType::Modify;
                            hasType = true;
                            logger.log(LogLevel::DEBUG, "File closed after write: " + fullPath, "InotifyWatcher");
                            metrics.incrementFilesModified();
                        }
                    }

                    if (hasType && callback_) {
                        WatchEvent ev{type, fullPath, std::nullopt, isDir};
                        logger.log(LogLevel::INFO, "Detected filesystem change: " + fullPath + 
                                   (isDir ? " (directory)" : " (file)"), "InotifyWatcher");
                        callback_(ev);
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
    
    logger.log(LogLevel::DEBUG, "Monitor loop ended", "InotifyWatcher");
}

} // namespace SentinelFS
