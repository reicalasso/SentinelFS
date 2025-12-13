#include "InotifyWatcher.h"
#include "Logger.h"
#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>
#include <climits>

namespace SentinelFS {
namespace IronRoot {

InotifyWatcher::InotifyWatcher() = default;

InotifyWatcher::~InotifyWatcher() {
    shutdown();
}

bool InotifyWatcher::initialize(WatchCallback callback) {
    auto& logger = Logger::instance();
    
    callback_ = callback;
    
    inotifyFd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotifyFd_ < 0) {
        logger.log(LogLevel::ERROR, "Failed to initialize inotify: " + std::string(strerror(errno)), "InotifyWatcher");
        return false;
    }
    
    running_ = true;
    watcherThread_ = std::thread(&InotifyWatcher::monitorLoop, this);
    
    logger.log(LogLevel::INFO, "inotify watcher initialized", "InotifyWatcher");
    return true;
}

void InotifyWatcher::shutdown() {
    auto& logger = Logger::instance();
    
    running_ = false;
    
    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
    
    if (inotifyFd_ >= 0) {
        close(inotifyFd_);
        inotifyFd_ = -1;
    }
    
    std::lock_guard<std::mutex> lock(watchMutex_);
    watchDescriptors_.clear();
    pathToWd_.clear();
    
    logger.log(LogLevel::INFO, "inotify watcher shut down", "InotifyWatcher");
}

bool InotifyWatcher::addWatch(const std::string& path) {
    auto& logger = Logger::instance();
    
    if (inotifyFd_ < 0) {
        return false;
    }
    
    // Check if already watching
    {
        std::lock_guard<std::mutex> lock(watchMutex_);
        if (pathToWd_.count(path) > 0) {
            return true;
        }
    }
    
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | 
                    IN_CLOSE_WRITE | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF;
    
    int wd = inotify_add_watch(inotifyFd_, path.c_str(), mask);
    if (wd < 0) {
        logger.log(LogLevel::ERROR, "Failed to add inotify watch for " + path + ": " + 
                   std::string(strerror(errno)), "InotifyWatcher");
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(watchMutex_);
        watchDescriptors_[wd] = path;
        pathToWd_[path] = wd;
    }
    
    // Individual watch logs suppressed to prevent spam - see addWatchRecursive for summary
    return true;
}

bool InotifyWatcher::removeWatch(const std::string& path) {
    auto& logger = Logger::instance();
    
    std::lock_guard<std::mutex> lock(watchMutex_);
    
    auto it = pathToWd_.find(path);
    if (it == pathToWd_.end()) {
        return false;
    }
    
    int wd = it->second;
    inotify_rm_watch(inotifyFd_, wd);
    
    watchDescriptors_.erase(wd);
    pathToWd_.erase(it);
    
    // Watch removed
    return true;
}

void InotifyWatcher::addWatchRecursive(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    
    if (!fs::exists(path, ec)) {
        return;
    }
    
    size_t initialCount = getWatchCount();
    addWatch(path);
    
    if (fs::is_directory(path, ec)) {
        for (auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); ++it) {
            if (ec) break;
            if (it->is_directory(ec)) {
                addWatch(it->path().string());
            }
        }
    }
    
    // Log summary instead of per-directory
    size_t addedCount = getWatchCount() - initialCount;
    if (addedCount > 0) {
        auto& logger = Logger::instance();
        logger.log(LogLevel::DEBUG, "Added " + std::to_string(addedCount) + " inotify watches for " + path, "InotifyWatcher");
    }
}

bool InotifyWatcher::isWatching(const std::string& path) const {
    std::lock_guard<std::mutex> lock(watchMutex_);
    return pathToWd_.count(path) > 0;
}

size_t InotifyWatcher::getWatchCount() const {
    std::lock_guard<std::mutex> lock(watchMutex_);
    return watchDescriptors_.size();
}

std::string InotifyWatcher::getWatchPath(int wd) const {
    std::lock_guard<std::mutex> lock(watchMutex_);
    auto it = watchDescriptors_.find(wd);
    return it != watchDescriptors_.end() ? it->second : "";
}

void InotifyWatcher::monitorLoop() {
    auto& logger = Logger::instance();
    
    const size_t EVENT_SIZE = sizeof(struct inotify_event);
    const size_t BUF_LEN = 4096 * (EVENT_SIZE + NAME_MAX + 1);
    char buffer[BUF_LEN];
    
    // For tracking renames
    uint32_t lastCookie = 0;
    std::string lastMovedFrom;
    
    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(inotifyFd_, &fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms
        
        int ret = select(inotifyFd_ + 1, &fds, nullptr, nullptr, &timeout);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            if (running_) {
                logger.log(LogLevel::ERROR, "inotify select error: " + std::string(strerror(errno)), "InotifyWatcher");
            }
            break;
        }
        
        if (ret == 0) continue;  // Timeout
        
        ssize_t len = read(inotifyFd_, buffer, BUF_LEN);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (running_) {
                logger.log(LogLevel::ERROR, "inotify read error: " + std::string(strerror(errno)), "InotifyWatcher");
            }
            break;
        }
        
        size_t i = 0;
        while (i < static_cast<size_t>(len)) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
            
            if (event->len > 0) {
                std::string dirPath = getWatchPath(event->wd);
                if (!dirPath.empty()) {
                    std::string filename = event->name;
                    std::string fullPath = dirPath + "/" + filename;
                    
                    WatchEventType type;
                    bool hasType = false;
                    bool isDir = (event->mask & IN_ISDIR) != 0;
                    std::optional<std::string> oldPath;
                    
                    if (event->mask & IN_CREATE) {
                        type = WatchEventType::Create;
                        hasType = true;
                        
                        // Auto-add watch for new directories
                        if (isDir) {
                            addWatchRecursive(fullPath);
                        }
                    } else if (event->mask & IN_DELETE) {
                        type = WatchEventType::Delete;
                        hasType = true;
                    } else if (event->mask & IN_MODIFY) {
                        if (!isDir) {
                            type = WatchEventType::Modify;
                            hasType = true;
                        }
                    } else if (event->mask & IN_CLOSE_WRITE) {
                        if (!isDir) {
                            type = WatchEventType::Modify;
                            hasType = true;
                        }
                    } else if (event->mask & IN_ATTRIB) {
                        type = WatchEventType::AttribChange;
                        hasType = true;
                    } else if (event->mask & IN_MOVED_FROM) {
                        lastCookie = event->cookie;
                        lastMovedFrom = fullPath;
                    } else if (event->mask & IN_MOVED_TO) {
                        type = WatchEventType::Rename;
                        hasType = true;
                        
                        if (event->cookie == lastCookie && !lastMovedFrom.empty()) {
                            oldPath = lastMovedFrom;
                        }
                        lastMovedFrom.clear();
                        
                        // Auto-add watch for moved directories
                        if (isDir) {
                            addWatchRecursive(fullPath);
                        }
                    }
                    
                    if (hasType && callback_) {
                        WatchEvent ev;
                        ev.type = type;
                        ev.path = fullPath;
                        ev.oldPath = oldPath;
                        ev.isDirectory = isDir;
                        ev.pid = 0;  // inotify doesn't provide PID
                        
                        callback_(ev);
                    }
                }
            }
            
            i += EVENT_SIZE + event->len;
        }
    }
}

} // namespace IronRoot
} // namespace SentinelFS
