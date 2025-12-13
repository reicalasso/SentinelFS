#pragma once

/**
 * @file ConflictHandler.h
 * @brief Conflict detection and resolution for sync pipeline
 * 
 * Handles conflict detection after hash scan and coordinates
 * with the merge resolver to resolve conflicts.
 */

#include "SyncProtocol.h"
#include "MergeResolver.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace SentinelFS {

class DatabaseManager;
class IStorageAPI;

namespace Sync {

/**
 * @brief Simple file information for sync operations
 */
struct SyncFile {
    std::string filePath;
    std::string fileId;
    std::string hash;
    std::string deviceId;
    uint64_t size;
    uint64_t modifiedTime;
    bool deleted;
    
    bool operator<(const SyncFile& other) const {
        return filePath < other.filePath;
    }
};

/**
 * @brief Conflict type enumeration
 */
enum class ConflictType {
    NONE = 0,
    CONTENT = 1,
    METADATA = 2,
    DELETION = 3,
    RENAME = 4
};

/**
 * @brief Conflict status enumeration
 */
enum class ConflictStatus {
    PENDING = 0,
    RESOLVED = 1,
    IGNORED = 2
};

/**
 * @brief File conflict information
 */
struct FileConflict {
    std::string filePath;
    std::string fileId;
    std::string localHash;
    std::string remoteHash;
    std::string baseHash;
    uint64_t localVersion;
    uint64_t remoteVersion;
    uint64_t baseVersion;
    ConflictType type;
    ConflictStatus status;
    std::string localDeviceId;
    std::string remoteDeviceId;
    uint64_t localModifiedTime;
    uint64_t remoteModifiedTime;
};

/**
 * @brief Conflict resolution request
 */
struct ConflictResolution {
    std::string conflictId;
    MergeStrategy strategy;
    std::string mergedContent;
    bool resolved;
};

/**
 * @brief Conflict handler for sync pipeline
 */
class ConflictHandler {
public:
    /**
     * @brief Constructor
     * @param dbManager Database manager instance
     * @param storage Storage API instance
     */
    ConflictHandler(DatabaseManager* dbManager, IStorageAPI* storage);
    
    ~ConflictHandler();
    
    /**
     * @brief Detect conflicts after hash scan
     * @param sessionId Sync session ID
     * @param localFiles Local file list
     * @param remoteFiles Remote file list
     * @return List of detected conflicts
     */
    std::vector<FileConflict> detectConflicts(
        const std::string& sessionId,
        const std::vector<SyncFile>& localFiles,
        const std::vector<SyncFile>& remoteFiles
    );
    
    /**
     * @brief Resolve a conflict using specified strategy
     * @param conflictId Conflict ID
     * @param resolution Resolution strategy and content
     * @return True if resolved successfully
     */
    bool resolveConflict(
        const std::string& conflictId,
        const ConflictResolution& resolution
    );
    
    /**
     * @brief Get pending conflicts for a session
     * @param sessionId Sync session ID
     * @return List of pending conflicts
     */
    std::vector<FileConflict> getPendingConflicts(const std::string& sessionId);
    
    /**
     * @brief Get conflict details
     * @param conflictId Conflict ID
     * @return Conflict information
     */
    std::optional<FileConflict> getConflict(const std::string& conflictId);
    
    /**
     * @brief Ignore a conflict
     * @param conflictId Conflict ID
     * @return True if ignored successfully
     */
    bool ignoreConflict(const std::string& conflictId);
    
    /**
     * @brief Attempt automatic conflict resolution
     * @param conflictId Conflict ID
     * @return True if auto-resolved
     */
    bool autoResolveConflict(const std::string& conflictId);
    
    /**
     * @brief Get merge preview for a conflict
     * @param conflictId Conflict ID
     * @return Merge result with conflicts
     */
    MergeResult getMergePreview(const std::string& conflictId);
    
    /**
     * @brief Callback for conflict detection
     */
    using ConflictDetectedCallback = std::function<void(const FileConflict&)>;
    
    /**
     * @brief Callback for conflict resolution
     */
    using ConflictResolvedCallback = std::function<void(const std::string&, bool)>;
    
    /**
     * @brief Set conflict detection callback
     */
    void setConflictDetectedCallback(ConflictDetectedCallback callback);
    
    /**
     * @brief Set conflict resolution callback
     */
    void setConflictResolvedCallback(ConflictResolvedCallback callback);
    
private:
    DatabaseManager* dbManager_;
    IStorageAPI* storage_;
    
    ConflictDetectedCallback onConflictDetected_;
    ConflictResolvedCallback onConflictResolved_;
    
    // Internal methods
    std::string createConflictRecord(const FileConflict& conflict);
    void updateConflictStatus(const std::string& conflictId, ConflictStatus status);
    std::string getFileContent(const std::string& filePath, const std::string& hash);
    std::vector<std::string> findCommonAncestors(
        const std::string& localHash,
        const std::string& remoteHash
    );
    bool isBinaryFile(const std::string& filePath);
    ConflictType determineConflictType(
        const SyncFile& local,
        const SyncFile& remote,
        const std::string& baseHash
    );
    
    // Helper method stubs
    uint64_t getFileVersion(const std::string& fileId, const std::string& deviceId);
    uint64_t getFileVersionByHash(const std::string& hash);
    bool wasFileDeletedRemotely(const std::string& fileId, const std::string& deviceId);
    bool wasFileDeletedLocally(const std::string& fileId, const std::string& deviceId);
    bool applyLocalVersion(const std::string& conflictId);
    bool applyRemoteVersion(const std::string& conflictId);
    bool performAutoMerge(const std::string& conflictId);
    bool applyMergedContent(const std::string& conflictId, const std::string& mergedContent);
    void createMergeResultRecord(const std::string& conflictId, const ConflictResolution& resolution);
};

} // namespace Sync
} // namespace SentinelFS
