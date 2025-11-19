#include "InotifyWatcher.h"
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
    callback_ = callback;
    
    inotifyFd_ = inotify_init();
    if (inotifyFd_ < 0) {
        std::cerr << "Failed to initialize inotify" << std::endl;
        return false;
    }

    running_ = true;
    watcherThread_ = std::thread(&InotifyWatcher::monitorLoop, this);
    return true;
}

void InotifyWatcher::shutdown() {
    running_ = false;
    if (inotifyFd_ >= 0) {
        close(inotifyFd_);
        inotifyFd_ = -1;
    }
    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
}

bool InotifyWatcher::addWatch(const std::string& path) {
    int wd = inotify_add_watch(inotifyFd_, path.c_str(), 
                                IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd < 0) {
        std::cerr << "Failed to add watch for " << path << std::endl;
        return false;
    }
    
    watchDescriptors_[wd] = path;
    std::cout << "Watching directory: " << path << std::endl;
    return true;
}

bool InotifyWatcher::removeWatch(const std::string& path) {
    for (auto it = watchDescriptors_.begin(); it != watchDescriptors_.end(); ++it) {
        if (it->second == path) {
            inotify_rm_watch(inotifyFd_, it->first);
            watchDescriptors_.erase(it);
            return true;
        }
    }
    return false;
}

void InotifyWatcher::monitorLoop() {
    const int EVENT_SIZE = sizeof(struct inotify_event);
    const int BUF_LEN = 1024 * (EVENT_SIZE + 16);
    char buffer[BUF_LEN];

    while (running_) {
        int length = read(inotifyFd_, buffer, BUF_LEN);
        if (length < 0) {
            if (running_) {
                std::cerr << "inotify read error" << std::endl;
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
                    
                    std::string eventType;
                    if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
                        eventType = "FILE_CREATED";
                    } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                        eventType = "FILE_DELETED";
                    } else if (event->mask & IN_MODIFY) {
                        eventType = "FILE_MODIFIED";
                    }

                    if (!eventType.empty() && callback_) {
                        std::cout << "Detected " << eventType << ": " << fullPath << std::endl;
                        callback_(eventType, fullPath);
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
}

} // namespace SentinelFS
