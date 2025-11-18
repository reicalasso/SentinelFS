#include "watcher_linux.h"
#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <iostream>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

namespace sfs {
namespace plugins {

WatcherLinux::WatcherLinux()
    : inotify_fd_(-1)
    , running_(false) {
}

WatcherLinux::~WatcherLinux() {
    stop();
}

bool WatcherLinux::start(const std::string& path) {
    if (running_) {
        return false;
    }
    
    // Initialize inotify
    inotify_fd_ = inotify_init();
    if (inotify_fd_ < 0) {
        std::cerr << "Failed to initialize inotify" << std::endl;
        return false;
    }
    
    watch_path_ = path;
    
    // Add recursive watches
    add_recursive_watches(path);
    
    if (watch_descriptors_.empty()) {
        close(inotify_fd_);
        inotify_fd_ = -1;
        return false;
    }
    
    // Start watch thread
    running_ = true;
    watch_thread_ = std::thread(&WatcherLinux::watch_loop, this);
    
    return true;
}

void WatcherLinux::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Wake up the blocking read by closing the fd
    if (inotify_fd_ >= 0) {
        close(inotify_fd_);
        inotify_fd_ = -1;
    }
    
    // Wait for thread to finish
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
    
    watch_descriptors_.clear();
}

bool WatcherLinux::is_running() const {
    return running_;
}

std::string WatcherLinux::get_watched_path() const {
    return watch_path_;
}

void WatcherLinux::watch_loop() {
    char buffer[EVENT_BUF_LEN];
    
    while (running_) {
        int length = read(inotify_fd_, buffer, EVENT_BUF_LEN);
        
        if (length < 0) {
            if (running_) {
                std::cerr << "inotify read error" << std::endl;
            }
            break;
        }
        
        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            
            // Get base path from watch descriptor
            auto it = watch_descriptors_.find(event->wd);
            if (it != watch_descriptors_.end()) {
                process_event(event, it->second);
            }
            
            i += EVENT_SIZE + event->len;
        }
    }
}

bool WatcherLinux::add_watch(const std::string& path) {
    uint32_t mask = IN_CREATE | IN_MODIFY | IN_DELETE | 
                    IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;
    
    int wd = inotify_add_watch(inotify_fd_, path.c_str(), mask);
    if (wd < 0) {
        return false;
    }
    
    watch_descriptors_[wd] = path;
    return true;
}

void WatcherLinux::remove_watch(int wd) {
    inotify_rm_watch(inotify_fd_, wd);
    watch_descriptors_.erase(wd);
}

void WatcherLinux::process_event(const struct inotify_event* event, const std::string& base_path) {
    if (!on_event) {
        return;
    }
    
    std::string filename = event->len > 0 ? event->name : "";
    std::string full_path = base_path;
    if (!filename.empty()) {
        if (full_path.back() != '/') {
            full_path += '/';
        }
        full_path += filename;
    }
    
    FsEvent fs_event;
    fs_event.path = full_path;
    fs_event.is_directory = (event->mask & IN_ISDIR) != 0;
    
    // Map inotify events to FsEventType
    if (event->mask & IN_CREATE) {
        fs_event.type = FsEventType::CREATED;
        
        // If a directory was created, add watch for it
        if (fs_event.is_directory && running_) {
            add_recursive_watches(full_path);
        }
    }
    else if (event->mask & IN_MODIFY) {
        fs_event.type = FsEventType::MODIFIED;
    }
    else if (event->mask & IN_DELETE) {
        fs_event.type = FsEventType::DELETED;
    }
    else if (event->mask & IN_MOVED_FROM) {
        fs_event.type = FsEventType::RENAMED_OLD;
    }
    else if (event->mask & IN_MOVED_TO) {
        fs_event.type = FsEventType::RENAMED_NEW;
        
        // If a directory was moved in, add watch for it
        if (fs_event.is_directory && running_) {
            add_recursive_watches(full_path);
        }
    }
    else if (event->mask & IN_ATTRIB) {
        fs_event.type = FsEventType::MODIFIED;
    }
    else {
        fs_event.type = FsEventType::UNKNOWN;
    }
    
    // Call the callback
    on_event(fs_event);
}

void WatcherLinux::add_recursive_watches(const std::string& path) {
    // Add watch for this directory
    if (!add_watch(path)) {
        return;
    }
    
    // Open directory
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return;
    }
    
    // Iterate through entries
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string full_path = path;
        if (full_path.back() != '/') {
            full_path += '/';
        }
        full_path += entry->d_name;
        
        // Check if it's a directory
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            // Recursively add watches for subdirectory
            add_recursive_watches(full_path);
        }
    }
    
    closedir(dir);
}

} // namespace plugins
} // namespace sfs
