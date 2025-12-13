#include "FileOps.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/xattr.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <openssl/evp.h>
#include <cstring>
#include <random>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace SentinelFS {
namespace IronRoot {

// Static members
std::map<std::string, int> FileOps::lockedFiles_;
std::mutex FileOps::lockMutex_;

// ThreadPool Implementation
ThreadPool::ThreadPool(size_t numThreads) : stop_(false) {
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4; // Fallback
    }
    
    for (size_t i = 0; i < numThreads; ++i) {
        threads_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    
                    if (stop_ && tasks_.empty()) return;
                    
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using return_type = decltype(f(args...));
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        if (stop_) {
            throw std::runtime_error("submit on stopped ThreadPool");
        }
        
        tasks_.emplace([task]() { (*task)(); });
    }
    
    condition_.notify_one();
    return result;
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
    
    for (std::thread& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    threads_.clear();
}

// FileOps Implementation
std::vector<uint8_t> FileOps::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    return data;
}

std::vector<uint8_t> FileOps::readFileMapped(const std::string& path, size_t maxSize) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return {};
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return {};
    }
    
    size_t size = static_cast<size_t>(st.st_size);
    if (maxSize > 0 && size > maxSize) {
        size = maxSize;
    }
    
    if (size == 0) {
        close(fd);
        return {};
    }
    
    void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (mapped == MAP_FAILED) {
        // Fallback to regular read
        return readFile(path);
    }
    
    // Advise kernel about sequential access
    madvise(mapped, size, MADV_SEQUENTIAL);
    
    std::vector<uint8_t> data(static_cast<uint8_t*>(mapped), 
                              static_cast<uint8_t*>(mapped) + size);
    
    munmap(mapped, size);
    
    return data;
}

bool FileOps::writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool FileOps::writeFileAtomic(const std::string& path, const std::vector<uint8_t>& data) {
    namespace fs = std::filesystem;
    
    // Generate temp file name in same directory
    fs::path targetPath(path);
    fs::path dir = targetPath.parent_path();
    std::string filename = targetPath.filename().string();
    
    // Create random suffix
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);
    std::string tempName = "." + filename + "." + std::to_string(dis(gen)) + ".tmp";
    fs::path tempPath = dir / tempName;
    
    // Write to temp file
    int fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return false;
    }
    
    ssize_t written = write(fd, data.data(), data.size());
    if (written < 0 || static_cast<size_t>(written) != data.size()) {
        close(fd);
        unlink(tempPath.c_str());
        return false;
    }
    
    // Sync to disk
    if (fsync(fd) < 0) {
        close(fd);
        unlink(tempPath.c_str());
        return false;
    }
    
    close(fd);
    
    // Atomic rename
    if (rename(tempPath.c_str(), path.c_str()) < 0) {
        unlink(tempPath.c_str());
        return false;
    }
    
    // Sync parent directory
    syncDirectory(dir.string());
    
    return true;
}

FileInfo FileOps::getFileInfo(const std::string& path) {
    FileInfo info;
    info.path = path;
    
    struct stat st;
    if (lstat(path.c_str(), &st) < 0) {
        return info;
    }
    
    info.size = st.st_size;
    info.mtime = st.st_mtime;
    info.mode = st.st_mode;
    info.uid = st.st_uid;
    info.gid = st.st_gid;
    info.isSymlink = S_ISLNK(st.st_mode);
    
    if (info.isSymlink) {
        info.symlinkTarget = readSymlink(path);
    }
    
    // Calculate hash for regular files
    if (S_ISREG(st.st_mode)) {
        info.hash = calculateHash(path);
    }
    
    // Get xattrs
    info.xattrs = getXattrs(path);
    
    return info;
}

std::string FileOps::calculateHash(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return "";
    }
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        close(fd);
        return "";
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        close(fd);
        return "";
    }
    
    char buffer[65536];
    ssize_t bytesRead;
    
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytesRead) != 1) {
            EVP_MD_CTX_free(ctx);
            close(fd);
            return "";
        }
    }
    
    close(fd);
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    
    if (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    EVP_MD_CTX_free(ctx);
    
    std::ostringstream oss;
    for (unsigned int i = 0; i < hashLen; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return oss.str();
}

std::string FileOps::calculateHash(const std::vector<uint8_t>& data) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    
    if (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    EVP_MD_CTX_free(ctx);
    
    std::ostringstream oss;
    for (unsigned int i = 0; i < hashLen; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return oss.str();
}

// Extended Attributes

std::map<std::string, std::string> FileOps::getXattrs(const std::string& path) {
    std::map<std::string, std::string> result;
    
    auto names = listXattrs(path);
    for (const auto& name : names) {
        auto value = getXattr(path, name);
        if (value.has_value()) {
            result[name] = value.value();
        }
    }
    
    return result;
}

std::optional<std::string> FileOps::getXattr(const std::string& path, const std::string& name) {
    // First get size
    ssize_t size = getxattr(path.c_str(), name.c_str(), nullptr, 0);
    if (size < 0) {
        return std::nullopt;
    }
    
    std::string value(size, '\0');
    ssize_t ret = getxattr(path.c_str(), name.c_str(), value.data(), size);
    if (ret < 0) {
        return std::nullopt;
    }
    
    value.resize(ret);
    return value;
}

bool FileOps::setXattr(const std::string& path, const std::string& name, const std::string& value) {
    return setxattr(path.c_str(), name.c_str(), value.data(), value.size(), 0) == 0;
}

bool FileOps::removeXattr(const std::string& path, const std::string& name) {
    return removexattr(path.c_str(), name.c_str()) == 0;
}

std::vector<std::string> FileOps::listXattrs(const std::string& path) {
    std::vector<std::string> result;
    
    // Get size of name list
    ssize_t size = listxattr(path.c_str(), nullptr, 0);
    if (size <= 0) {
        return result;
    }
    
    std::string buffer(size, '\0');
    ssize_t ret = listxattr(path.c_str(), buffer.data(), size);
    if (ret <= 0) {
        return result;
    }
    
    // Parse null-separated list
    size_t start = 0;
    for (size_t i = 0; i < static_cast<size_t>(ret); ++i) {
        if (buffer[i] == '\0') {
            if (i > start) {
                result.push_back(buffer.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    
    return result;
}

// File Locking

bool FileOps::lockFile(const std::string& path, bool exclusive, bool blocking) {
    std::lock_guard<std::mutex> lock(lockMutex_);
    
    // Check if already locked by us
    if (lockedFiles_.count(path) > 0) {
        return true;
    }
    
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) {
        return false;
    }
    
    int operation = exclusive ? LOCK_EX : LOCK_SH;
    if (!blocking) {
        operation |= LOCK_NB;
    }
    
    if (flock(fd, operation) < 0) {
        close(fd);
        return false;
    }
    
    lockedFiles_[path] = fd;
    return true;
}

bool FileOps::unlockFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(lockMutex_);
    
    auto it = lockedFiles_.find(path);
    if (it == lockedFiles_.end()) {
        return false;
    }
    
    flock(it->second, LOCK_UN);
    close(it->second);
    lockedFiles_.erase(it);
    
    return true;
}

bool FileOps::isFileLocked(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    
    // Try non-blocking exclusive lock
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        close(fd);
        return true;  // File is locked
    }
    
    // Got the lock, release it
    flock(fd, LOCK_UN);
    close(fd);
    return false;
}

// Symlink operations

bool FileOps::isSymlink(const std::string& path) {
    struct stat st;
    return lstat(path.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

std::string FileOps::readSymlink(const std::string& path) {
    char buf[PATH_MAX];
    ssize_t len = ::readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (len < 0) {
        return "";
    }
    buf[len] = '\0';
    return std::string(buf);
}

bool FileOps::createSymlink(const std::string& target, const std::string& link) {
    return symlink(target.c_str(), link.c_str()) == 0;
}

// Utility

bool FileOps::supportsXattr(const std::string& path) {
    // Try to list xattrs - if it fails with ENOTSUP, xattrs not supported
    ssize_t ret = listxattr(path.c_str(), nullptr, 0);
    if (ret < 0 && errno == ENOTSUP) {
        return false;
    }
    return true;
}

std::string FileOps::getFilesystemType(const std::string& path) {
    struct statvfs st;
    if (statvfs(path.c_str(), &st) < 0) {
        return "";
    }
    
    // statvfs doesn't give us filesystem type directly
    // We need to read /proc/mounts
    std::ifstream mounts("/proc/mounts");
    std::string line;
    std::string result;
    size_t longestMatch = 0;
    
    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        std::string device, mountpoint, fstype;
        iss >> device >> mountpoint >> fstype;
        
        if (path.find(mountpoint) == 0 && mountpoint.size() > longestMatch) {
            longestMatch = mountpoint.size();
            result = fstype;
        }
    }
    
    return result;
}

bool FileOps::syncFile(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    
    int ret = fsync(fd);
    close(fd);
    return ret == 0;
}

bool FileOps::syncDirectory(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return false;
    }
    
    int ret = fsync(fd);
    close(fd);
    return ret == 0;
}

// Parallel Scanning Operations

std::pair<std::vector<FileInfo>, ScanStats> FileOps::scanDirectoryParallel(
    const std::string& rootPath, 
    const ScanConfig& config) {
    
    auto startTime = std::chrono::high_resolution_clock::now();
    ScanStats stats;
    std::vector<FileInfo> allFiles;
    
    // Determine thread count
    size_t threadCount = config.maxThreads;
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 4;
    }
    
    stats.threadsUsed = threadCount;
    
    // Create thread pool
    auto threadPool = std::make_shared<ThreadPool>(threadCount);
    
    try {
        // Start parallel scan
        auto paths = scanDirectorySingle(rootPath, config, threadPool);
        
        // Process files in parallel batches
        std::vector<std::future<FileInfo>> futures;
        
        for (const auto& path : paths) {
            if (shouldIgnorePath(path, config.ignorePatterns)) {
                continue;
            }
            
            // Submit file processing task
            auto future = threadPool->submit(&FileOps::processFile, path, config);
            futures.push_back(std::move(future));
            
            // Process in batches to avoid memory issues
            if (futures.size() >= config.batchSize) {
                for (auto& f : futures) {
                    auto fileInfo = f.get();
                    if (fileInfo.size > 0 || fileInfo.isSymlink) {
                        allFiles.push_back(fileInfo);
                        stats.totalSize += fileInfo.size;
                        
                        if (fileInfo.isSymlink) {
                            stats.totalSymlinks++;
                        } else {
                            stats.totalFiles++;
                        }
                        
                        if (fileInfo.isHardLink) {
                            stats.hardLinks++;
                        }
                    }
                }
                futures.clear();
            }
        }
        
        // Process remaining files
        for (auto& f : futures) {
            auto fileInfo = f.get();
            if (fileInfo.size > 0 || fileInfo.isSymlink) {
                allFiles.push_back(fileInfo);
                stats.totalSize += fileInfo.size;
                
                if (fileInfo.isSymlink) {
                    stats.totalSymlinks++;
                } else {
                    stats.totalFiles++;
                }
                
                if (fileInfo.isHardLink) {
                    stats.hardLinks++;
                }
            }
        }
        
        // Count directories (approximate)
        stats.totalDirectories = std::count_if(allFiles.begin(), allFiles.end(),
            [](const FileInfo& info) { return info.mode & S_IFDIR; });
        
        // Calculate scan time
        auto endTime = std::chrono::high_resolution_clock::now();
        stats.scanTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        
        threadPool->shutdown();
        
    } catch (const std::exception& e) {
        threadPool->shutdown();
        throw;
    }
    
    return {allFiles, stats};
}

std::vector<FileInfo> FileOps::batchProcessFiles(
    const std::vector<FileInfo>& files,
    const ScanConfig& config) {
    
    std::vector<FileInfo> processedFiles;
    auto threadPool = std::make_shared<ThreadPool>(config.maxThreads);
    
    try {
        std::vector<std::future<FileInfo>> futures;
        
        for (const auto& file : files) {
            // Skip if already has hash and not needed
            if (!file.hash.empty() && !config.includeXattrs) {
                processedFiles.push_back(file);
                continue;
            }
            
            auto future = threadPool->submit(&FileOps::processFile, file.path, config);
            futures.push_back(std::move(future));
            
            if (futures.size() >= config.batchSize) {
                for (auto& f : futures) {
                    processedFiles.push_back(f.get());
                }
                futures.clear();
            }
        }
        
        for (auto& f : futures) {
            processedFiles.push_back(f.get());
        }
        
        threadPool->shutdown();
        
    } catch (const std::exception& e) {
        threadPool->shutdown();
        throw;
    }
    
    return processedFiles;
}

std::map<uint64_t, std::vector<std::string>> FileOps::detectHardLinks(
    const std::vector<FileInfo>& files) {
    
    std::map<uint64_t, std::vector<std::string>> hardLinks;
    
    for (const auto& file : files) {
        if (file.isHardLink && file.inode > 0) {
            hardLinks[file.inode].push_back(file.path);
        }
    }
    
    // Remove single entries (not actually hard links)
    for (auto it = hardLinks.begin(); it != hardLinks.end();) {
        if (it->second.size() < 2) {
            it = hardLinks.erase(it);
        } else {
            ++it;
        }
    }
    
    return hardLinks;
}

bool FileOps::shouldIgnorePath(
    const std::string& path,
    const std::vector<std::string>& patterns) {
    
    for (const auto& pattern : patterns) {
        if (fnmatch(pattern.c_str(), path.c_str(), FNM_PATHNAME) == 0) {
            return true;
        }
    }
    
    return false;
}

// Internal helper methods

std::vector<std::string> FileOps::scanDirectorySingle(
    const std::string& path,
    const ScanConfig& config,
    std::shared_ptr<ThreadPool> threadPool) {
    
    std::vector<std::string> allPaths;
    std::vector<std::future<std::vector<std::string>>> futures;
    
    // Get directory entries
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return allPaths;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        
        // Skip . and ..
        if (name == "." || name == "..") {
            continue;
        }
        
        std::string fullPath = path + "/" + name;
        
        // Check if should ignore
        if (shouldIgnorePath(fullPath, config.ignorePatterns)) {
            continue;
        }
        
        allPaths.push_back(fullPath);
        
        // If directory, scan recursively
        if (entry->d_type == DT_DIR || 
            (entry->d_type == DT_UNKNOWN && isDirectory(fullPath))) {
            
            // Submit recursive scan to thread pool
            auto future = threadPool->submit(&FileOps::scanDirectorySingle, 
                                           fullPath, config, threadPool);
            futures.push_back(std::move(future));
        }
    }
    
    closedir(dir);
    
    // Collect results from recursive scans
    for (auto& f : futures) {
        auto subPaths = f.get();
        allPaths.insert(allPaths.end(), subPaths.begin(), subPaths.end());
    }
    
    return allPaths;
}

FileInfo FileOps::processFile(
    const std::string& path,
    const ScanConfig& config) {
    
    FileInfo info;
    info.path = path;
    
    struct stat st;
    if (lstat(path.c_str(), &st) < 0) {
        return info;
    }
    
    info.size = st.st_size;
    info.mtime = st.st_mtime;
    info.mode = st.st_mode;
    info.uid = st.st_uid;
    info.gid = st.st_gid;
    info.inode = st.st_ino;
    
    // Check for symlinks
    if (S_ISLNK(st.st_mode)) {
        info.isSymlink = true;
        info.symlinkTarget = readSymlink(path);
        return info;
    }
    
    // Check for hard links
    if (st.st_nlink > 1) {
        info.isHardLink = true;
    }
    
    // Calculate hash for regular files
    if (S_ISREG(st.st_mode)) {
        // Use memory mapping for large files
        if (st.st_size > config.largeFileThreshold) {
            info.hash = calculateHash(path);
        } else {
            auto data = readFile(path);
            if (!data.empty()) {
                info.hash = calculateHash(data);
            }
        }
    }
    
    // Get extended attributes if requested
    if (config.includeXattrs) {
        info.xattrs = getXattrs(path);
    }
    
    return info;
}

bool FileOps::isDirectory(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace IronRoot
} // namespace SentinelFS
