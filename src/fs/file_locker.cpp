#include "file_locker.hpp"
#include <iostream>
#include <sys/stat.h>

FileLocker::FileLocker() = default;

FileLocker::~FileLocker() {
    // Release all locks when the locker is destroyed
    std::lock_guard<std::mutex> lock(lockerMutex);
    for (auto& pair : activeLocks) {
        flock(pair.second.fd, LOCK_UN);
        close(pair.second.fd);
    }
}

bool FileLocker::acquireLock(const std::string& filepath, LockType type, 
                           std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(lockerMutex);
    
    // Check if file is already locked
    if (activeLocks.find(filepath) != activeLocks.end()) {
        // Check if it's the same thread trying to re-lock
        if (activeLocks[filepath].lockerId == std::this_thread::get_id()) {
            // This thread already has the lock
            return true;
        }
        // File is locked by another thread
        return false;
    }
    
    // Get file descriptor
    int fd = getFileDescriptor(filepath);
    if (fd == -1) {
        return false;
    }
    
    // Try to acquire the lock with timeout
    int lockResult = 0;
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        if (type == LockType::WRITE) {
            lockResult = flock(fd, LOCK_EX | LOCK_NB);  // Exclusive lock, non-blocking
        } else {
            lockResult = flock(fd, LOCK_SH | LOCK_NB);  // Shared lock, non-blocking
        }
        
        if (lockResult == 0) {
            // Lock acquired successfully
            activeLocks[filepath] = FileLockInfo(fd, type);
            return true;
        }
        
        // Check if timeout has been reached
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        
        if (elapsed >= timeout) {
            close(fd); // Close the file descriptor if we couldn't acquire the lock
            return false;
        }
        
        // Brief sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool FileLocker::releaseLock(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(lockerMutex);
    
    auto it = activeLocks.find(filepath);
    if (it == activeLocks.end()) {
        // File is not locked by us
        return false;
    }
    
    // Release the lock
    flock(it->second.fd, LOCK_UN);
    
    // Close the file descriptor
    close(it->second.fd);
    
    // Remove from active locks
    activeLocks.erase(it);
    
    return true;
}

bool FileLocker::isLocked(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(lockerMutex);
    return activeLocks.find(filepath) != activeLocks.end();
}

LockType FileLocker::getLockType(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(lockerMutex);
    auto it = activeLocks.find(filepath);
    if (it != activeLocks.end()) {
        return it->second.type;
    }
    // Return invalid type if not locked
    return LockType::READ; // Actually means "not locked", but using READ as default
}

bool FileLocker::forceUnlock(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(lockerMutex);
    
    auto it = activeLocks.find(filepath);
    if (it == activeLocks.end()) {
        // File is not locked by us
        return false;
    }
    
    // Release the lock
    flock(it->second.fd, LOCK_UN);
    
    // Close the file descriptor
    close(it->second.fd);
    
    // Remove from active locks
    activeLocks.erase(it);
    
    return true;
}

void FileLocker::cleanupStaleLocks(std::chrono::seconds maxAge) {
    std::lock_guard<std::mutex> lock(lockerMutex);
    
    auto now = std::chrono::steady_clock::now();
    for (auto it = activeLocks.begin(); it != activeLocks.end();) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.acquiredAt);
        
        if (duration > maxAge) {
            // Release stale lock
            flock(it->second.fd, LOCK_UN);
            close(it->second.fd);
            it = activeLocks.erase(it);
        } else {
            ++it;
        }
    }
}

int FileLocker::getFileDescriptor(const std::string& filepath, bool createIfNotExists) {
    int flags = O_RDWR;
    if (createIfNotExists) {
        flags |= O_CREAT;
    }
    
    int fd = open(filepath.c_str(), flags, 0644);
    if (fd == -1 && createIfNotExists) {
        // Try with more permissive permissions if file doesn't exist
        fd = open(filepath.c_str(), flags | O_CREAT, 0666);
    }
    
    return fd;
}

void FileLocker::cleanupFileDescriptor(int fd) {
    close(fd);
}