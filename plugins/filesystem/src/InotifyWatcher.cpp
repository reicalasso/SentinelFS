#include "InotifyWatcher.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <sys/inotify.h>
#include <unistd.h>
#include <cstring>

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
    
    inotifyFd_ = inotify_init();
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
    if (inotifyFd_ >= 0) {
        close(inotifyFd_);
        inotifyFd_ = -1;
    }
    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
    logger.log(LogLevel::INFO, "Inotify watcher shut down", "InotifyWatcher");
}

bool InotifyWatcher::addWatch(const std::string& path) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Adding watch for: " + path, "InotifyWatcher");
    
    int wd = inotify_add_watch(inotifyFd_, path.c_str(), 
                                IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd < 0) {
        logger.log(LogLevel::ERROR, "Failed to add watch for " + path + ": " + std::string(strerror(errno)), "InotifyWatcher");
        metrics.incrementSyncErrors();
        return false;
    }
    
    watchDescriptors_[wd] = path;
    logger.log(LogLevel::INFO, "Now watching directory: " + path, "InotifyWatcher");
    metrics.incrementFilesWatched();
    return true;
}

bool InotifyWatcher::removeWatch(const std::string& path) {
    auto& logger = Logger::instance();
    
    logger.log(LogLevel::DEBUG, "Removing watch for: " + path, "InotifyWatcher");
    
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

void InotifyWatcher::monitorLoop() {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Monitor loop started", "InotifyWatcher");
    
    const int EVENT_SIZE = sizeof(struct inotify_event);
    const int BUF_LEN = 1024 * (EVENT_SIZE + 16);
    char buffer[BUF_LEN];

    while (running_) {
        int length = read(inotifyFd_, buffer, BUF_LEN);
        if (length < 0) {
            if (running_) {
                logger.log(LogLevel::ERROR, "Inotify read error: " + std::string(strerror(errno)), "InotifyWatcher");
                metrics.incrementSyncErrors();
            }
            break;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            if (event->len) {
                auto it = watchDescriptors_.find(event->wd);
                if (it != watchDescriptors_.end()) {
                    std::string dirPath = it->second;
                    std::string filename = event->name;
                    std::string fullPath = dirPath + "/" + filename;
                    
                    WatchEventType type;
                    bool hasType = false;

                    if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {
                        type = WatchEventType::Create;
                        hasType = true;
                    } else if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) {
                        type = WatchEventType::Delete;
                        hasType = true;
                        metrics.incrementFilesDeleted();
                    } else if (event->mask & IN_MODIFY) {
                        type = WatchEventType::Modify;
                        hasType = true;
                        metrics.incrementFilesModified();
                    }

                    if (hasType && callback_) {
                        WatchEvent ev{type, fullPath, std::nullopt};
                        logger.log(LogLevel::INFO, "Detected filesystem change: " + fullPath, "InotifyWatcher");
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
