#pragma once

#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

enum class LockType {
    READ,
    WRITE
};

struct FileLockInfo {
    int fd;  // File descriptor
    LockType type;
    std::thread::id lockerId;
    std::chrono::steady_clock::time_point acquiredAt;
    
    FileLockInfo() : fd(-1), type(LockType::READ), 
                     lockerId(std::thread::id()), 
                     acquiredAt(std::chrono::steady_clock::time_point::min()) {}  // Default constructor
    FileLockInfo(int fileDesc, LockType lockT) 
        : fd(fileDesc), type(lockT), lockerId(std::this_thread::get_id()),
          acquiredAt(std::chrono::steady_clock::now()) {}
};

class FileLocker {
public:
    FileLocker();
    ~FileLocker();
    
    // Acquire a lock on a file
    bool acquireLock(const std::string& filepath, LockType type, 
                    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Release a lock on a file
    bool releaseLock(const std::string& filepath);
    
    // Check if file is currently locked
    bool isLocked(const std::string& filepath) const;
    
    // Get lock type for a file
    LockType getLockType(const std::string& filepath) const;
    
    // Force unlock a file (may be dangerous)
    bool forceUnlock(const std::string& filepath);
    
    // Cleanup stale locks (locks held longer than a timeout)
    void cleanupStaleLocks(std::chrono::seconds maxAge = std::chrono::seconds(300)); // 5 minutes
    
private:
    mutable std::mutex lockerMutex;
    std::map<std::string, FileLockInfo> activeLocks;
    
    // Helper method to get a file descriptor
    int getFileDescriptor(const std::string& filepath, bool createIfNotExists = true);
    
    // Helper method to close and cleanup file descriptor
    void cleanupFileDescriptor(int fd);
};