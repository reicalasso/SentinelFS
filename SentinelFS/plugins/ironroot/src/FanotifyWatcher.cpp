#include "FanotifyWatcher.h"
#include "Logger.h"
#include <sys/fanotify.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace SentinelFS {
namespace IronRoot {

FanotifyWatcher::FanotifyWatcher() = default;

FanotifyWatcher::~FanotifyWatcher() {
    shutdown();
}

bool FanotifyWatcher::isAvailable() {
    // Try to initialize fanotify - requires CAP_SYS_ADMIN
    int fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF | FAN_NONBLOCK, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return true;
    }
    return false;
}

bool FanotifyWatcher::initialize(WatchCallback callback) {
    auto& logger = Logger::instance();
    
    callback_ = callback;
    
    // Initialize fanotify with reporting class
    // FAN_REPORT_FID gives us file handle info
    // FAN_REPORT_DIR_FID gives us directory info
    fanotifyFd_ = fanotify_init(
        FAN_CLOEXEC | FAN_CLASS_NOTIF | FAN_NONBLOCK | FAN_REPORT_FID | FAN_REPORT_DIR_FID,
        O_RDONLY | O_LARGEFILE
    );
    
    if (fanotifyFd_ < 0) {
        // Try without FAN_REPORT_* flags (older kernels)
        fanotifyFd_ = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF | FAN_NONBLOCK, O_RDONLY);
        if (fanotifyFd_ < 0) {
            logger.log(LogLevel::ERROR, "Failed to initialize fanotify: " + std::string(strerror(errno)) +
                       " (requires CAP_SYS_ADMIN)", "FanotifyWatcher");
            return false;
        }
    }
    
    running_ = true;
    watcherThread_ = std::thread(&FanotifyWatcher::monitorLoop, this);
    
    logger.log(LogLevel::INFO, "fanotify watcher initialized", "FanotifyWatcher");
    return true;
}

void FanotifyWatcher::shutdown() {
    auto& logger = Logger::instance();
    
    running_ = false;
    
    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
    
    if (fanotifyFd_ >= 0) {
        close(fanotifyFd_);
        fanotifyFd_ = -1;
    }
    
    std::lock_guard<std::mutex> lock(watchMutex_);
    watchedPaths_.clear();
    
    logger.log(LogLevel::INFO, "fanotify watcher shut down", "FanotifyWatcher");
}

bool FanotifyWatcher::addWatch(const std::string& path) {
    auto& logger = Logger::instance();
    
    if (fanotifyFd_ < 0) {
        return false;
    }
    
    // Mark the path for monitoring
    uint64_t mask = FAN_CREATE | FAN_DELETE | FAN_MODIFY | FAN_MOVED_FROM | FAN_MOVED_TO |
                    FAN_CLOSE_WRITE | FAN_ONDIR | FAN_EVENT_ON_CHILD;
    
    int ret = fanotify_mark(fanotifyFd_, FAN_MARK_ADD | FAN_MARK_ONLYDIR, mask, AT_FDCWD, path.c_str());
    if (ret < 0) {
        // Try without FAN_MARK_ONLYDIR for files
        ret = fanotify_mark(fanotifyFd_, FAN_MARK_ADD, mask, AT_FDCWD, path.c_str());
        if (ret < 0) {
            logger.log(LogLevel::ERROR, "Failed to add fanotify mark for " + path + ": " + 
                       std::string(strerror(errno)), "FanotifyWatcher");
            return false;
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(watchMutex_);
        watchedPaths_.insert(path);
    }
    
    logger.log(LogLevel::DEBUG, "Added fanotify watch: " + path, "FanotifyWatcher");
    return true;
}

bool FanotifyWatcher::removeWatch(const std::string& path) {
    auto& logger = Logger::instance();
    
    if (fanotifyFd_ < 0) {
        return false;
    }
    
    uint64_t mask = FAN_CREATE | FAN_DELETE | FAN_MODIFY | FAN_MOVED_FROM | FAN_MOVED_TO |
                    FAN_CLOSE_WRITE | FAN_ONDIR | FAN_EVENT_ON_CHILD;
    
    int ret = fanotify_mark(fanotifyFd_, FAN_MARK_REMOVE, mask, AT_FDCWD, path.c_str());
    if (ret < 0) {
        logger.log(LogLevel::WARN, "Failed to remove fanotify mark for " + path, "FanotifyWatcher");
    }
    
    {
        std::lock_guard<std::mutex> lock(watchMutex_);
        watchedPaths_.erase(path);
    }
    
    return ret >= 0;
}

void FanotifyWatcher::addWatchRecursive(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    
    if (!fs::exists(path, ec)) {
        return;
    }
    
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
}

bool FanotifyWatcher::isWatching(const std::string& path) const {
    std::lock_guard<std::mutex> lock(watchMutex_);
    return watchedPaths_.count(path) > 0;
}

size_t FanotifyWatcher::getWatchCount() const {
    std::lock_guard<std::mutex> lock(watchMutex_);
    return watchedPaths_.size();
}

std::string FanotifyWatcher::getProcessName(pid_t pid) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(procCacheMutex_);
        auto it = processNameCache_.find(pid);
        if (it != processNameCache_.end()) {
            return it->second;
        }
    }
    
    // Read from /proc
    std::string commPath = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream file(commPath);
    std::string name;
    if (file && std::getline(file, name)) {
        // Remove trailing newline
        if (!name.empty() && name.back() == '\n') {
            name.pop_back();
        }
        
        // Cache it
        std::lock_guard<std::mutex> lock(procCacheMutex_);
        processNameCache_[pid] = name;
        return name;
    }
    
    return "";
}

std::string FanotifyWatcher::getProcessPath(pid_t pid) {
    char buf[PATH_MAX];
    std::string exePath = "/proc/" + std::to_string(pid) + "/exe";
    ssize_t len = readlink(exePath.c_str(), buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "";
}

void FanotifyWatcher::monitorLoop() {
    auto& logger = Logger::instance();
    
    const size_t EVENT_BUF_SIZE = 8192;
    alignas(struct fanotify_event_metadata) char buffer[EVENT_BUF_SIZE];
    
    while (running_) {
        struct pollfd pfd;
        pfd.fd = fanotifyFd_;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, 100);  // 100ms timeout
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            if (running_) {
                logger.log(LogLevel::ERROR, "fanotify poll error: " + std::string(strerror(errno)), "FanotifyWatcher");
            }
            break;
        }
        
        if (ret == 0) continue;  // Timeout
        
        ssize_t len = read(fanotifyFd_, buffer, sizeof(buffer));
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (running_) {
                logger.log(LogLevel::ERROR, "fanotify read error: " + std::string(strerror(errno)), "FanotifyWatcher");
            }
            break;
        }
        
        // Process events
        struct fanotify_event_metadata* metadata = reinterpret_cast<struct fanotify_event_metadata*>(buffer);
        
        while (FAN_EVENT_OK(metadata, len)) {
            if (metadata->vers != FANOTIFY_METADATA_VERSION) {
                logger.log(LogLevel::ERROR, "fanotify metadata version mismatch", "FanotifyWatcher");
                break;
            }
            
            if (metadata->fd >= 0) {
                // Get file path from fd
                char pathBuf[PATH_MAX];
                std::string fdPath = "/proc/self/fd/" + std::to_string(metadata->fd);
                ssize_t pathLen = readlink(fdPath.c_str(), pathBuf, sizeof(pathBuf) - 1);
                
                if (pathLen > 0) {
                    pathBuf[pathLen] = '\0';
                    std::string filePath(pathBuf);
                    
                    // Determine event type
                    WatchEventType type;
                    bool hasType = false;
                    bool isDir = (metadata->mask & FAN_ONDIR) != 0;
                    
                    if (metadata->mask & FAN_CREATE) {
                        type = WatchEventType::Create;
                        hasType = true;
                    } else if (metadata->mask & FAN_DELETE) {
                        type = WatchEventType::Delete;
                        hasType = true;
                    } else if (metadata->mask & FAN_MODIFY) {
                        type = WatchEventType::Modify;
                        hasType = true;
                    } else if (metadata->mask & FAN_CLOSE_WRITE) {
                        type = WatchEventType::Modify;
                        hasType = true;
                    } else if ((metadata->mask & FAN_MOVED_FROM) || (metadata->mask & FAN_MOVED_TO)) {
                        type = WatchEventType::Rename;
                        hasType = true;
                    }
                    
                    if (hasType && callback_) {
                        WatchEvent event;
                        event.type = type;
                        event.path = filePath;
                        event.isDirectory = isDir;
                        event.pid = metadata->pid;
                        event.processName = getProcessName(metadata->pid);
                        
                        callback_(event);
                    }
                }
                
                close(metadata->fd);
            }
            
            metadata = FAN_EVENT_NEXT(metadata, len);
        }
    }
}

} // namespace IronRoot
} // namespace SentinelFS
