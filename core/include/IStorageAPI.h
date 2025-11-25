#pragma once

#include "IPlugin.h"
#include <string>
#include <optional>
#include <vector>

namespace SentinelFS {

    struct FileMetadata {
        std::string path;
        std::string hash;
        long long timestamp;
        long long size;
        std::string vectorClock;  // Serialized vector clock for conflict detection
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
         */
        virtual bool markConflictResolved(int conflictId) = 0;
        
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
         */
        virtual void* getDB() = 0;
    };
}


