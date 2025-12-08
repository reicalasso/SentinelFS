#pragma once

/**
 * @file FileOps.h
 * @brief Advanced file operations for IronRoot
 * 
 * Features:
 * - Memory-mapped I/O
 * - Atomic writes
 * - Extended attributes
 * - File locking
 * - Hash calculation
 */

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <optional>
#include <mutex>

namespace SentinelFS {
namespace IronRoot {

/**
 * @brief Extended file information
 */
struct FileInfo {
    std::string path;
    std::string hash;
    uint64_t size{0};
    uint64_t mtime{0};
    uint32_t mode{0};
    uint32_t uid{0};
    uint32_t gid{0};
    bool isSymlink{false};
    std::string symlinkTarget;
    std::map<std::string, std::string> xattrs;
};

/**
 * @brief File operations utility class
 */
class FileOps {
public:
    /**
     * @brief Read file contents
     */
    static std::vector<uint8_t> readFile(const std::string& path);
    
    /**
     * @brief Read file using memory mapping (efficient for large files)
     * @param path File path
     * @param maxSize Maximum size to map (0 = entire file)
     */
    static std::vector<uint8_t> readFileMapped(const std::string& path, size_t maxSize = 0);
    
    /**
     * @brief Write file contents
     */
    static bool writeFile(const std::string& path, const std::vector<uint8_t>& data);
    
    /**
     * @brief Write file atomically (write to temp, fsync, rename)
     * This ensures the file is never in a partial state
     */
    static bool writeFileAtomic(const std::string& path, const std::vector<uint8_t>& data);
    
    /**
     * @brief Get extended file information
     */
    static FileInfo getFileInfo(const std::string& path);
    
    /**
     * @brief Calculate file hash (SHA-256)
     */
    static std::string calculateHash(const std::string& path);
    
    /**
     * @brief Calculate hash of data
     */
    static std::string calculateHash(const std::vector<uint8_t>& data);
    
    // Extended Attributes (xattr)
    
    /**
     * @brief Get all extended attributes
     */
    static std::map<std::string, std::string> getXattrs(const std::string& path);
    
    /**
     * @brief Get a specific extended attribute
     */
    static std::optional<std::string> getXattr(const std::string& path, const std::string& name);
    
    /**
     * @brief Set an extended attribute
     */
    static bool setXattr(const std::string& path, const std::string& name, const std::string& value);
    
    /**
     * @brief Remove an extended attribute
     */
    static bool removeXattr(const std::string& path, const std::string& name);
    
    /**
     * @brief List extended attribute names
     */
    static std::vector<std::string> listXattrs(const std::string& path);
    
    // File Locking
    
    /**
     * @brief Acquire file lock
     * @param exclusive true for write lock, false for read lock
     * @param blocking true to wait for lock, false to fail immediately
     */
    static bool lockFile(const std::string& path, bool exclusive = true, bool blocking = true);
    
    /**
     * @brief Release file lock
     */
    static bool unlockFile(const std::string& path);
    
    /**
     * @brief Check if file is locked
     */
    static bool isFileLocked(const std::string& path);
    
    // Symlink operations
    
    /**
     * @brief Check if path is a symlink
     */
    static bool isSymlink(const std::string& path);
    
    /**
     * @brief Get symlink target
     */
    static std::string readSymlink(const std::string& path);
    
    /**
     * @brief Create symlink
     */
    static bool createSymlink(const std::string& target, const std::string& link);
    
    // Utility
    
    /**
     * @brief Check if xattr is supported on path
     */
    static bool supportsXattr(const std::string& path);
    
    /**
     * @brief Get filesystem type for path
     */
    static std::string getFilesystemType(const std::string& path);
    
    /**
     * @brief Sync file to disk
     */
    static bool syncFile(const std::string& path);
    
    /**
     * @brief Sync directory to disk
     */
    static bool syncDirectory(const std::string& path);

private:
    // Lock tracking
    static std::map<std::string, int> lockedFiles_;
    static std::mutex lockMutex_;
};

} // namespace IronRoot
} // namespace SentinelFS
