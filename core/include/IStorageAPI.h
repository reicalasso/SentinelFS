#pragma once

#include "IPlugin.h"
#include <string>
#include <optional>
#include <vector>
#include <functional>

namespace SentinelFS {

    struct FileMetadata {
        std::string path;
        std::string hash;
        long long timestamp;
        long long size;
        std::string vectorClock;  // Serialized vector clock for conflict detection
        int synced{0};
        int version{1};
    };

    struct PeerInfo {
        std::string id;
        std::string ip;
        int port;
        long long lastSeen;
        std::string status; // "active", "offline"
        int latency; // RTT in milliseconds, -1 if not measured
    };
    
    struct ConflictInfo {
        int id;
        std::string path;
        std::string localHash;
        std::string remoteHash;
        std::string remotePeerId;
        long long localTimestamp;
        long long remoteTimestamp;
        long long localSize;
        long long remoteSize;
        int strategy;  // ResolutionStrategy as int
        bool resolved;
        long long detectedAt;
        long long resolvedAt;
    };
    
    struct WatchedFolder {
        int id;
        std::string path;
        long long addedAt;
        int statusId;  // 1 = active, 0 = inactive
    };
    
    struct SyncQueueItem {
        int id;
        std::string filePath;
        std::string opType;
        std::string status;
        int priority;
        long long createdAt;
    };
    
    struct ThreatInfo {
        int id;
        std::string filePath;
        std::string threatType;
        std::string threatLevel;
        double threatScore;
        std::string detectedAt;
        std::string description;
        bool markedSafe;
    };
    
    struct ActivityLogEntry {
        int id;
        std::string filePath;
        std::string opType;
        long long timestamp;
        std::string peerId;
        std::string details;
    };

    class IStorageAPI : public IPlugin {
    public:
        virtual ~IStorageAPI() = default;

        // --- File Operations ---

        /**
         * @brief Add or update file metadata in the storage.
         */
        virtual bool addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) = 0;

        /**
         * @brief Retrieve file metadata by path.
         */
        virtual std::optional<FileMetadata> getFile(const std::string& path) = 0;

        /**
         * @brief Remove file metadata by path.
         */
        virtual bool removeFile(const std::string& path) = 0;

        // --- Peer Operations ---

        /**
         * @brief Add or update a peer in the storage.
         */
        virtual bool addPeer(const PeerInfo& peer) = 0;

        /**
         * @brief Get a peer by ID.
         */
        virtual std::optional<PeerInfo> getPeer(const std::string& peerId) = 0;

        /**
         * @brief Get all known peers.
         */
        virtual std::vector<PeerInfo> getAllPeers() = 0;

        /**
         * @brief Update peer latency.
         * @param peerId The ID of the peer.
         * @param latency RTT in milliseconds.
         */
        virtual bool updatePeerLatency(const std::string& peerId, int latency) = 0;

        /**
         * @brief Get all peers sorted by latency (lowest first).
         * @return Vector of peers sorted by latency, offline/unknown latency peers at end.
         */
        virtual std::vector<PeerInfo> getPeersByLatency() = 0;
        
        /**
         * @brief Remove a peer by ID.
         */
        virtual bool removePeer(const std::string& peerId) = 0;
        
        // --- Conflict Operations ---
        
        /**
         * @brief Record a detected conflict.
         */
        virtual bool addConflict(const ConflictInfo& conflict) = 0;
        
        /**
         * @brief Get all unresolved conflicts.
         */
        virtual std::vector<ConflictInfo> getUnresolvedConflicts() = 0;
        
        /**
         * @brief Get all conflicts for a specific file.
         */
        virtual std::vector<ConflictInfo> getConflictsForFile(const std::string& path) = 0;
        
        /**
         * @brief Mark a conflict as resolved.
         * @param conflictId The conflict ID
         * @param strategy Resolution strategy (0=local, 1=remote, 2=both)
         */
        virtual bool markConflictResolved(int conflictId, int strategy = 0) = 0;
        
        /**
         * @brief Get conflict statistics.
         * @return Pair of (total_conflicts, unresolved_conflicts)
         */
        virtual std::pair<int, int> getConflictStats() = 0;

        // --- Sync Queue / Access Log (minimal API) ---

        /**
         * @brief Enqueue a file operation into the sync queue.
         * @param filePath Path of the file.
         * @param opType Operation type (e.g. "create", "update", "delete").
         * @param status Initial status (e.g. "pending").
         */
        virtual bool enqueueSyncOperation(const std::string& filePath,
                                          const std::string& opType,
                                          const std::string& status) = 0;

        /**
         * @brief Append a record to the file access log.
         * @param filePath Path of the file.
         * @param opType Operation type (e.g. "read", "write").
         * @param deviceId Optional device identifier (may be empty).
         * @param timestamp Event time in milliseconds since epoch.
         */
        virtual bool logFileAccess(const std::string& filePath,
                                   const std::string& opType,
                                   const std::string& deviceId,
                                   long long timestamp) = 0;
        
        /**
         * @brief Get direct access to SQLite database handle
         * @return sqlite3* database handle
         * @deprecated Use specific API methods instead for proper statistics tracking
         */
        virtual void* getDB() = 0;
        
        // --- Watched Folder Operations ---
        
        /**
         * @brief Add a folder to watch list
         */
        virtual bool addWatchedFolder(const std::string& path) = 0;
        
        /**
         * @brief Remove a folder from watch list
         */
        virtual bool removeWatchedFolder(const std::string& path) = 0;
        
        /**
         * @brief Get all active watched folders
         */
        virtual std::vector<WatchedFolder> getWatchedFolders() = 0;
        
        /**
         * @brief Check if a folder is being watched
         */
        virtual bool isWatchedFolder(const std::string& path) = 0;
        
        /**
         * @brief Update watched folder status
         */
        virtual bool updateWatchedFolderStatus(const std::string& path, int statusId) = 0;
        
        // --- Bulk File Operations ---
        
        /**
         * @brief Get all files in a folder (recursive)
         */
        virtual std::vector<FileMetadata> getFilesInFolder(const std::string& folderPath) = 0;
        
        /**
         * @brief Remove all files in a folder from database
         * @return Number of files removed
         */
        virtual int removeFilesInFolder(const std::string& folderPath) = 0;
        
        /**
         * @brief Get total file count
         */
        virtual int getFileCount() = 0;
        
        /**
         * @brief Get total size of all files
         */
        virtual long long getTotalFileSize() = 0;
        
        /**
         * @brief Mark file as synced
         */
        virtual bool markFileSynced(const std::string& path, bool synced = true) = 0;
        
        /**
         * @brief Get pending (unsynced) files
         */
        virtual std::vector<FileMetadata> getPendingFiles() = 0;
        
        // --- Ignore Patterns ---
        
        /**
         * @brief Add an ignore pattern
         */
        virtual bool addIgnorePattern(const std::string& pattern) = 0;
        
        /**
         * @brief Remove an ignore pattern
         */
        virtual bool removeIgnorePattern(const std::string& pattern) = 0;
        
        /**
         * @brief Get all ignore patterns
         */
        virtual std::vector<std::string> getIgnorePatterns() = 0;
        
        // --- Threat Operations ---
        
        /**
         * @brief Add a detected threat
         */
        virtual bool addThreat(const ThreatInfo& threat) = 0;
        
        /**
         * @brief Get all detected threats
         */
        virtual std::vector<ThreatInfo> getThreats() = 0;
        
        /**
         * @brief Remove a threat
         */
        virtual bool removeThreat(int threatId) = 0;
        
        /**
         * @brief Remove all threats for files in a folder
         */
        virtual int removeThreatsInFolder(const std::string& folderPath) = 0;
        
        /**
         * @brief Mark threat as safe (false positive)
         */
        virtual bool markThreatSafe(int threatId, bool safe = true) = 0;
        
        // --- Sync Queue Operations ---
        
        /**
         * @brief Get pending sync operations
         */
        virtual std::vector<SyncQueueItem> getSyncQueue() = 0;
        
        /**
         * @brief Update sync queue item status
         */
        virtual bool updateSyncQueueStatus(int itemId, const std::string& status) = 0;
        
        /**
         * @brief Remove completed sync operations
         */
        virtual int clearCompletedSyncOperations() = 0;
        
        // --- Activity Log ---
        
        /**
         * @brief Get recent activity
         */
        virtual std::vector<ActivityLogEntry> getRecentActivity(int limit = 50) = 0;
        
        // --- Peer Extended Operations ---
        
        /**
         * @brief Remove all peers
         */
        virtual bool removeAllPeers() = 0;
        
        /**
         * @brief Update peer status
         */
        virtual bool updatePeerStatus(const std::string& peerId, const std::string& status) = 0;
        
        /**
         * @brief Block a peer
         */
        virtual bool blockPeer(const std::string& peerId) = 0;
        
        /**
         * @brief Unblock a peer
         */
        virtual bool unblockPeer(const std::string& peerId) = 0;
        
        /**
         * @brief Check if peer is blocked
         */
        virtual bool isPeerBlocked(const std::string& peerId) = 0;
        
        // --- Config/Settings Storage ---
        
        /**
         * @brief Store a config value
         */
        virtual bool setConfig(const std::string& key, const std::string& value) = 0;
        
        /**
         * @brief Get a config value
         */
        virtual std::optional<std::string> getConfig(const std::string& key) = 0;
        
        /**
         * @brief Remove a config value
         */
        virtual bool removeConfig(const std::string& key) = 0;
        
        // --- Transfer History ---
        
        /**
         * @brief Log a transfer
         */
        virtual bool logTransfer(const std::string& filePath, const std::string& peerId, 
                                 const std::string& direction, long long bytes, bool success) = 0;
        
        /**
         * @brief Get transfer history
         */
        virtual std::vector<std::pair<std::string, long long>> getTransferHistory(int limit = 50) = 0;
    };
}


