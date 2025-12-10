#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <map>
#include <optional>
#include <filesystem>
#include <mutex>

namespace SentinelFS {

/**
 * @brief Metadata for a single file version
 */
struct FileVersion {
    uint64_t versionId;                         // Unique version identifier
    std::string filePath;                       // Original file path
    std::string versionPath;                    // Path to versioned file in storage
    std::string hash;                           // SHA-256 hash of content
    std::string peerId;                         // Peer that created this version (empty if local)
    uint64_t timestamp;                         // Unix timestamp (milliseconds)
    size_t size;                                // File size in bytes
    std::string changeType;                     // "create", "modify", "conflict"
    std::string comment;                        // Optional comment/description
    
    FileVersion() 
        : versionId(0), timestamp(0), size(0) {}
    
    bool operator<(const FileVersion& other) const {
        return timestamp < other.timestamp;
    }
};

/**
 * @brief Configuration for versioning behavior
 */
struct VersioningConfig {
    size_t maxVersionsPerFile = 10;             // Keep N most recent versions per file
    size_t maxTotalVersionsBytes = 1024 * 1024 * 500;  // 500 MB max total
    bool enableVersioning = true;
    bool versionOnConflict = true;              // Always create version on conflict
    bool versionOnRemoteChange = true;          // Version before accepting remote changes
    std::string versionStoragePath = ".sentinel_versions";  // Relative to watch dir
    
    // File patterns to exclude from versioning
    std::vector<std::string> excludePatterns = {
        "*.tmp", "*.swp", "*.lock", ".DS_Store", "Thumbs.db"
    };
};

/**
 * @brief File versioning manager - keeps N latest versions of files
 * 
 * Stores versioned files in a hidden directory structure:
 * .sentinel_versions/
 *   file_abc123/           <- hash of original path
 *     v_1234567890.ext     <- versioned file with timestamp
 *     v_1234567891.ext
 *     metadata.json        <- version metadata
 */
class FileVersionManager {
public:
    /**
     * @brief Initialize version manager
     * @param watchDirectory Base directory being watched
     * @param config Versioning configuration
     */
    explicit FileVersionManager(
        const std::string& watchDirectory,
        const VersioningConfig& config = VersioningConfig{}
    );
    
    ~FileVersionManager();
    
    /**
     * @brief Create a new version of a file
     * @param filePath Original file path
     * @param changeType Type of change ("modify", "conflict", etc.)
     * @param peerId Peer ID if from remote (empty for local)
     * @param comment Optional comment
     * @return Created version, or nullopt on failure
     */
    std::optional<FileVersion> createVersion(
        const std::string& filePath,
        const std::string& changeType = "modify",
        const std::string& peerId = "",
        const std::string& comment = ""
    );
    
    /**
     * @brief Create version from raw data (for remote versions)
     * @param filePath Original file path
     * @param data File content
     * @param hash SHA-256 hash
     * @param timestamp Original timestamp
     * @param peerId Source peer ID
     * @param changeType Type of change
     * @return Created version, or nullopt on failure
     */
    std::optional<FileVersion> createVersionFromData(
        const std::string& filePath,
        const std::vector<uint8_t>& data,
        const std::string& hash,
        uint64_t timestamp,
        const std::string& peerId,
        const std::string& changeType = "remote"
    );
    
    /**
     * @brief Get all versions of a file, sorted by timestamp (newest first)
     * @param filePath Original file path
     * @return Vector of versions
     */
    std::vector<FileVersion> getVersions(const std::string& filePath) const;
    
    /**
     * @brief Get specific version by ID
     * @param filePath Original file path
     * @param versionId Version ID
     * @return Version if found
     */
    std::optional<FileVersion> getVersion(
        const std::string& filePath,
        uint64_t versionId
    ) const;
    
    /**
     * @brief Get the latest version of a file
     * @param filePath Original file path
     * @return Latest version if exists
     */
    std::optional<FileVersion> getLatestVersion(const std::string& filePath) const;
    
    /**
     * @brief Restore a specific version to the original location
     * @param filePath Original file path
     * @param versionId Version ID to restore
     * @param createBackup Create backup of current file before restore
     * @return true if successful
     */
    bool restoreVersion(
        const std::string& filePath,
        uint64_t versionId,
        bool createBackup = true
    );
    
    /**
     * @brief Read version content
     * @param filePath Original file path
     * @param versionId Version ID
     * @return File content as bytes
     */
    std::optional<std::vector<uint8_t>> readVersionContent(
        const std::string& filePath,
        uint64_t versionId
    ) const;
    
    /**
     * @brief Delete a specific version
     * @param filePath Original file path
     * @param versionId Version ID to delete
     * @return true if deleted
     */
    bool deleteVersion(const std::string& filePath, uint64_t versionId);
    
    /**
     * @brief Delete all versions of a file
     * @param filePath Original file path
     * @return Number of versions deleted
     */
    size_t deleteAllVersions(const std::string& filePath);
    
    /**
     * @brief Prune old versions based on config limits
     * @param filePath If provided, only prune this file's versions
     * @return Number of versions pruned
     */
    size_t pruneVersions(const std::string& filePath = "");
    
    /**
     * @brief Get total storage used by versions
     * @return Total bytes used
     */
    size_t getTotalVersionStorageBytes() const;
    
    /**
     * @brief Get version count for a file
     * @param filePath Original file path
     * @return Number of versions
     */
    size_t getVersionCount(const std::string& filePath) const;
    
    /**
     * @brief Get all files with versions
     * @return Map of file paths to version counts
     */
    std::map<std::string, size_t> getVersionedFiles() const;
    
    /**
     * @brief Check if file should be excluded from versioning
     * @param filePath File path to check
     * @return true if excluded
     */
    bool isExcluded(const std::string& filePath) const;
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void setConfig(const VersioningConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current config
     */
    const VersioningConfig& getConfig() const { return config_; }
    
private:
    std::string watchDirectory_;
    VersioningConfig config_;
    std::string versionStoragePath_;
    mutable std::mutex mutex_;
    
    // Internal helpers
    std::string getVersionDirForFile(const std::string& filePath) const;
    std::string generateVersionFilename(const std::string& originalPath, uint64_t timestamp) const;
    std::string hashPath(const std::string& path) const;
    bool matchesPattern(const std::string& filename, const std::string& pattern) const;
    void loadMetadata(const std::string& versionDir, std::vector<FileVersion>& versions) const;
    void saveMetadata(const std::string& versionDir, const std::vector<FileVersion>& versions);
    uint64_t generateVersionId() const;
    void ensureVersionDirectory(const std::string& versionDir);
};

} // namespace SentinelFS
