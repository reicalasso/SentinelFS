#pragma once

#include "DBHelper.h"
#include "IStorageAPI.h"
#include <vector>
#include <string>
#include <map>

namespace SentinelFS {

// Forward declaration
class DatabaseManager;

/**
 * @brief Batch database operations using DatabaseManager
 */
class BatchQueries {
public:
    /**
     * @brief Information about a file operation
     */
    struct OperationInfo {
        int fileId;
        int deviceId;
        DBHelper::OpType opType;
        DBHelper::StatusType status;
        int64_t timestamp;
    };
    
    /**
     * @brief Information about a file
     */
    struct FileInfo {
        std::string path;
        std::string hash;
        int64_t size;
        int64_t modifiedTime;
    };
    
    /**
     * @brief Pending operation details
     */
    struct PendingOperation {
        int id;
        int fileId;
        std::string filePath;
        DBHelper::OpType opType;
        DBHelper::StatusType status;
        int64_t timestamp;
    };
    
    /**
     * @brief Batch upsert peers using storage API
     * @param storage Storage implementation
     * @param peers List of peers to upsert
     * @return Number of successful operations
     */
    static int batchUpsertPeers(IStorageAPI* storage, const std::vector<PeerInfo>& peers);
    
    /**
     * @brief Batch get peers using storage API
     * @param storage Storage implementation
     * @param peerIds List of peer IDs to get
     * @return Map of peer ID to PeerInfo
     */
    static std::map<std::string, PeerInfo> batchGetPeers(IStorageAPI* storage, const std::vector<std::string>& peerIds);
    
    /**
     * @brief Batch update peer latencies
     * @param storage Storage implementation
     * @param latencies Map of peer ID to latency
     * @return true if all updates succeeded
     */
    static bool batchUpdateLatencies(IStorageAPI* storage, const std::map<std::string, int>& latencies);
    
    /**
     * @brief Batch insert operations using DatabaseManager
     * @param db DatabaseManager instance
     * @param ops List of operations to insert
     * @return Number of successful insertions
     */
    static int batchInsertOperations(DatabaseManager* db, const std::vector<OperationInfo>& ops);
    
    /**
     * @brief Get pending operations
     * @param db DatabaseManager instance
     * @param limit Maximum number of operations to return
     * @return List of pending operations
     */
    static std::vector<PendingOperation> getPendingOperations(DatabaseManager* db, int limit = 100);
    
    /**
     * @brief Update operation status
     * @param db DatabaseManager instance
     * @param operationId Operation ID
     * @param newStatus New status
     * @return true if successful
     */
    static bool updateOperationStatus(DatabaseManager* db, int operationId, DBHelper::StatusType newStatus);
    
    /**
     * @brief Batch update file hashes
     * @param db DatabaseManager instance
     * @param files List of files to update
     * @return Number of successful updates
     */
    static int batchUpdateFileHashes(DatabaseManager* db, const std::vector<FileInfo>& files);
    
    /**
     * @brief Get orphaned files (files with no recent operations)
     * @param db DatabaseManager instance
     * @return List of orphaned file paths
     */
    static std::vector<std::string> getOrphanedFiles(DatabaseManager* db);
    
    /**
     * @brief Clean up old records
     * @param db DatabaseManager instance
     * @param daysToKeep Number of days to keep records
     * @return true if successful
     */
    static bool cleanupOldRecords(DatabaseManager* db, int daysToKeep = 30);
};

} // namespace SentinelFS
