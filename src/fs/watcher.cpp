#include "watcher.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sys/inotify.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

FileWatcher::FileWatcher(const std::string& path, EventCallback callback) 
    : watchPath(path), callback(callback), running(false) {
#ifdef __linux__
    inotifyFd = inotify_init();
    if (inotifyFd < 0) {
        std::cerr << "Error initializing inotify" << std::endl;
    } else {
        watchDescriptor = inotify_add_watch(inotifyFd, path.c_str(), 
                                           IN_MODIFY | IN_CREATE | IN_DELETE);
        if (watchDescriptor < 0) {
            std::cerr << "Error adding watch for path: " << path << std::endl;
        }
    }
#endif
}

FileWatcher::~FileWatcher() {
    stop();
#ifdef __linux__
    if (inotifyFd >= 0) {
        close(inotifyFd);
    }
#endif
}

void FileWatcher::start() {
    running = true;
    watchThread = std::thread(&FileWatcher::watchLoop, this);
}

void FileWatcher::stop() {
    running = false;
    watchThread.join();
}

void FileWatcher::watchLoop() {
#ifdef __linux__
    char buffer[4096];
    
    while (running) {
        int length = read(inotifyFd, buffer, sizeof(buffer));
        if (length < 0) {
            continue;
        }
        
        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            
            if (event->len) {  // If filename is present
                std::string filePath = watchPath + "/" + event->name;
                
                std::string eventType = "unknown";
                if (event->mask & IN_CREATE) eventType = "created";
                else if (event->mask & IN_DELETE) eventType = "deleted";
                else if (event->mask & IN_MODIFY) eventType = "modified";
                
                // For this basic implementation, get a rough file size
                size_t fileSize = 0; 
                
                FileEvent fileEvent(filePath, eventType, fileSize, 0);
                callback(fileEvent);
            }
            
            i += sizeof(struct inotify_event) + event->len;
        }
    }
#else
    // For other platforms, implement a polling-based approach
    // This is a simplified version that just periodically checks for file changes
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // In a real implementation for other platforms, 
        // you would use platform-specific file watching APIs
        // like FSEvents on macOS or ReadDirectoryChangesW on Windows
    }
#endif
}