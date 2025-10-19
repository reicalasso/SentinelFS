#pragma once

#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

// Forward declarations
class DeltaEngine;
class NetworkManager;

// File event structure
struct FileEvent {
    std::string path;
    std::string type; // "created", "modified", "deleted"
    size_t file_size;
    int peer_id;
    
    FileEvent(const std::string& p, const std::string& t, size_t size = 0, int pid = 0) 
        : path(p), type(t), file_size(size), peer_id(pid) {}
};

class FileWatcher {
public:
    using EventCallback = std::function<void(const FileEvent&)>;

    FileWatcher(const std::string& path, EventCallback callback);
    ~FileWatcher();

    void start();
    void stop();
    bool isRunning() const { return running; }

private:
    std::string watchPath;
    EventCallback callback;
    std::atomic<bool> running{false};
    std::thread watchThread;
    
    void watchLoop();

    // Platform-specific implementation
#ifdef _WIN32
    void* hDir;  // Handle to directory
#elif __linux__
    int inotifyFd;
    int watchDescriptor;
#else
    // macOS implementation would go here
#endif
};