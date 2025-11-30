#pragma once

/**
 * @file SocketGuard.h
 * @brief RAII wrapper for socket file descriptors
 * 
 * Ensures sockets are properly closed when going out of scope,
 * preventing resource leaks.
 */

#include <unistd.h>
#include <utility>

namespace sfs {

/**
 * @brief RAII wrapper for socket file descriptors
 * 
 * Usage:
 * @code
 * SocketGuard sock(socket(AF_INET, SOCK_STREAM, 0));
 * if (!sock) { // handle error }
 * connect(sock.get(), ...);
 * // Socket automatically closed when sock goes out of scope
 * 
 * // Or release ownership:
 * int fd = sock.release();  // Now caller owns the fd
 * @endcode
 */
class SocketGuard {
public:
    /// Create an empty guard (no socket)
    SocketGuard() noexcept : fd_(-1) {}
    
    /// Take ownership of a socket fd
    explicit SocketGuard(int fd) noexcept : fd_(fd) {}
    
    /// Non-copyable
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    
    /// Move constructor
    SocketGuard(SocketGuard&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    
    /// Move assignment
    SocketGuard& operator=(SocketGuard&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    
    /// Destructor - closes socket if owned
    ~SocketGuard() {
        reset();
    }
    
    /// Get the raw file descriptor
    int get() const noexcept { return fd_; }
    
    /// Check if guard holds a valid socket
    explicit operator bool() const noexcept { return fd_ >= 0; }
    
    /// Check if guard holds a valid socket
    bool valid() const noexcept { return fd_ >= 0; }
    
    /// Release ownership and return the fd
    int release() noexcept {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }
    
    /// Close current socket (if any) and take ownership of new one
    void reset(int fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }
    
    /// Swap with another guard
    void swap(SocketGuard& other) noexcept {
        std::swap(fd_, other.fd_);
    }

private:
    int fd_;
};

/// Swap two SocketGuards
inline void swap(SocketGuard& a, SocketGuard& b) noexcept {
    a.swap(b);
}

} // namespace sfs
