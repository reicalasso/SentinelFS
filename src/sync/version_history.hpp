#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <set>
#include <functional>
#include <thread>
#include "../models.hpp"

// File version information
struct FileVersion {
    std::string versionId;
    std::string filePath;
    std::string checksum;        // SHA256 hash of file content
    size_t fileSize;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastModified;
    std::string modifiedBy;     // Peer ID who last modified
    std::string commitMessage;   // Optional description of changes
    bool compressed;             // Whether version is compressed
    std::string compressionAlgorithm;  // If compressed, which algorithm
    std::set<std::string> tags; // Custom tags for version categorization
    
    FileVersion() : fileSize(0), compressed(false) {
        createdAt = std::chrono::system_clock::now();
        lastModified = createdAt;
    }
    
    FileVersion(const std::string& path, const std::string& hash = "")
        : filePath(path), checksum(hash), fileSize(0), compressed(false) {
        createdAt = std::chrono::system_clock::now();
        lastModified = createdAt;
    }
};

// Version policy for retention
struct VersionPolicy {
    bool enableVersioning;      // Enable/disable versioning
    size_t maxVersions;        // Maximum number of versions to keep per file
    std::chrono::hours maxAge; // Maximum age of versions (0 = no limit)
    std::vector<std::string> importantFilePatterns;  // Always keep versions of these files
    bool compressOldVersions;   // Compress old versions to save space
    std::string compressionAlgorithm;  // Algorithm for compression
    
    VersionPolicy() : enableVersioning(true), maxVersions(10), 
                     maxAge(std::chrono::hours(24*30)),  // 30 days
                     compressOldVersions(true), compressionAlgorithm("gzip") {}
};

// Version history manager
class VersionHistoryManager {
public:
    VersionHistoryManager(const VersionPolicy& policy = VersionPolicy());
    ~VersionHistoryManager();
    
    // Start/stop version management
    void start();
    void stop();
    
    // Version management
    FileVersion createFileVersion(const std::string& filePath, 
                                  const std::string& commitMessage = "",
                                  const std::string& modifiedBy = "");
    bool deleteFileVersion(const std::string& versionId);
    bool restoreFileVersion(const std::string& versionId, 
                           const std::string& restorePath = "");
    
    // Version queries
    std::vector<FileVersion> getFileVersions(const std::string& filePath) const;
    FileVersion getLatestVersion(const std::string& filePath) const;
    FileVersion getVersionById(const std::string& versionId) const;
    std::vector<FileVersion> getVersionsByDate(const std::chrono::system_clock::time_point& startDate,
                                               const std::chrono::system_clock::time_point& endDate) const;
    
    // Version comparison
    bool compareVersions(const std::string& versionId1, const std::string& versionId2) const;
    std::vector<std::pair<size_t, size_t>> getDiff(const std::string& versionId1, 
                                                   const std::string& versionId2) const;
    
    // Tags management
    void addVersionTag(const std::string& versionId, const std::string& tag);
    void removeVersionTag(const std::string& versionId, const std::string& tag);
    std::set<std::string> getVersionTags(const std::string& versionId) const;
    std::vector<FileVersion> getVersionsWithTag(const std::string& tag) const;
    
    // Policy management
    void setVersionPolicy(const VersionPolicy& policy);
    VersionPolicy getVersionPolicy() const;
    bool isVersionImportant(const FileVersion& version) const;
    
    // Automatic cleanup
    void enableAutomaticCleanup(bool enable);
    void cleanupOldVersions();
    void cleanupByVersionCount();
    void cleanupByAge();
    
    // Compression management
    bool compressVersion(const std::string& versionId);
    bool decompressVersion(const std::string& versionId);
    bool isVersionCompressed(const std::string& versionId) const;
    
    // Export/Import
    bool exportVersion(const std::string& versionId, const std::string& exportPath) const;
    FileVersion importVersion(const std::string& importPath);
    
    // Version metadata
    std::string calculateVersionChecksum(const std::string& filePath) const;
    size_t getVersionFileSize(const std::string& versionId) const;
    std::chrono::system_clock::time_point getVersionCreationTime(const std::string& versionId) const;
    
    // Bulk operations
    std::vector<FileVersion> getRecentVersions(size_t limit = 50) const;
    std::vector<FileVersion> searchVersions(const std::string& query) const;
    void deleteFileVersionRange(const std::string& filePath,
                               const std::chrono::system_clock::time_point& beforeDate);
    
    // Statistics
    size_t getTotalVersions() const;
    size_t getVersionsForFile(const std::string& filePath) const;
    std::map<std::string, size_t> getVersionStatistics() const;
    
    // Event callbacks
    using VersionCreatedCallback = std::function<void(const FileVersion&)>;
    using VersionDeletedCallback = std::function<void(const std::string& versionId)>;
    using VersionRestoredCallback = std::function<void(const FileVersion&, const std::string& restorePath)>;
    
    void setVersionCreatedCallback(VersionCreatedCallback callback);
    void setVersionDeletedCallback(VersionDeletedCallback callback);
    void setVersionRestoredCallback(VersionRestoredCallback callback);
    
private:
    VersionPolicy policy;
    mutable std::mutex versionsMutex;
    std::map<std::string, std::vector<FileVersion>> fileVersions;  // filePath -> versions
    std::map<std::string, FileVersion> versionIndex;              // versionId -> version
    std::map<std::string, std::set<std::string>> tagIndex;        // tag -> versionIds
    
    mutable std::mutex storageMutex;
    std::string versionStoragePath;  // Where version files are stored
    bool automaticCleanupEnabled;
    
    std::atomic<bool> running{false};
    std::thread cleanupThread;
    
    // Callbacks
    VersionCreatedCallback versionCreatedCallback;
    VersionDeletedCallback versionDeletedCallback;
    VersionRestoredCallback versionRestoredCallback;
    
    // Internal methods
    void cleanupLoop();
    void enforceVersionPolicy();
    std::string generateVersionId(const std::string& filePath) const;
    std::string getVersionStoragePath(const std::string& versionId) const;
    bool storeVersionFile(const FileVersion& version) const;
    bool retrieveVersionFile(const std::string& versionId, const std::string& destination) const;
    void notifyVersionCreated(const FileVersion& version);
    void notifyVersionDeleted(const std::string& versionId);
    void notifyVersionRestored(const FileVersion& version, const std::string& restorePath);
    
    // Utilities
    bool shouldKeepVersion(const FileVersion& version) const;
    std::vector<FileVersion> sortVersionsByDate(const std::vector<FileVersion>& versions) const;
    bool isValidVersionId(const std::string& versionId) const;
    std::string sanitizeFileName(const std::string& fileName) const;
};