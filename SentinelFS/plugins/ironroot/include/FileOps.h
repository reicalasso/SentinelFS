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
 * - Parallel directory scanning
 * - Batch operations
 */

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <optional>
#include <mutex>
#include <future>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>

namespace SentinelFS {
namespace IronRoot {

/**
 * @brief Scan configuration
 */
struct ScanConfig {
    size_t maxThreads{0};              // 0 = auto-detect
    size_t batchSize{100};             // Files per batch
    size_t largeFileThreshold{10 * 1024 * 1024}; // 10MB
    bool followSymlinks{false};
    bool includeXattrs{false};
    std::vector<std::string> ignorePatterns;
};

/**
 * @brief Scan statistics
 */
struct ScanStats {
    size_t totalFiles{0};
    size_t totalDirectories{0};
    size_t totalSymlinks{0};
    size_t totalSize{0};
    size_t hardLinks{0};
    std::chrono::milliseconds scanTime{0};
    size_t threadsUsed{0};
};

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
    bool isHardLink{false};
    uint64_t inode{0};
    std::string symlinkTarget;
    std::map<std::string, std::string> xattrs;
};

/**
 * @brief Thread pool for parallel operations
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();
    
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    
    void shutdown();
    size_t getThreadCount() const { return threads_.size(); }
    
private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_{false};
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
    
    // Parallel Scanning Operations
    
    /**
     * @brief Scan directory recursively with parallel processing
     * @param rootPath Root directory to scan
     * @param config Scan configuration
     * @return Pair of file list and scan statistics
     */
    static std::pair<std::vector<FileInfo>, ScanStats> scanDirectoryParallel(
        const std::string& rootPath, 
        const ScanConfig& config = ScanConfig{}
    );
    
    /**
     * @brief Batch process files for operations like hash calculation
     * @param files List of files to process
     * @param config Scan configuration
     * @return Processed files list
     */
    static std::vector<FileInfo> batchProcessFiles(
        const std::vector<FileInfo>& files,
        const ScanConfig& config = ScanConfig{}
    );
    
    /**
     * @brief Detect hard links in file list
     * @param files List of files to check
     * @return Map of inode to file paths (hard links)
     */
    static std::map<uint64_t, std::vector<std::string>> detectHardLinks(
        const std::vector<FileInfo>& files
    );
    
    /**
     * @brief Check if path should be ignored based on patterns
     * @param path Path to check
     * @param patterns Ignore patterns
     * @return True if should ignore
     */
    static bool shouldIgnorePath(
        const std::string& path,
        const std::vector<std::string>& patterns
    );
    
    // Locking Operations
    
    /**
     * @brief Acquire file lock with options
     * @param exclusive true for write lock, false for read lock
     * @param blocking true to wait for lock, false to fail immediately
     */
    static bool lockFile(const std::string& path, bool exclusive = true, bool blocking = true);
    
    /**
     * @brief Release lock on file
     */
    static bool unlockFile(const std::string& path);
    
    /**
     * @brief Check if path is a directory
     */
    static bool isDirectory(const std::string& path);
    
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
    
    /**
     * @brief Check if file is locked
     */
    static bool isFileLocked(const std::string& path);
    
    /**
     * @brief List extended attribute names
     */
    static std::vector<std::string> listXattrs(const std::string& path);
    
private:
    static std::map<std::string, int> lockedFiles_;
    static std::mutex lockMutex_;
    
    // Internal helper methods
    static std::vector<std::string> scanDirectorySingle(
        const std::string& path,
        const ScanConfig& config,
        std::shared_ptr<ThreadPool> threadPool
    );
    
    static FileInfo processFile(
        const std::string& path,
        const ScanConfig& config
    );
};

} // namespace IronRoot
} // namespace SentinelFS
