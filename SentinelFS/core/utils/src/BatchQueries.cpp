/**
 * @file BatchQueriesNew.cpp
 * @brief Implementation of batch database operations using DatabaseManager
 */

#include "BatchQueries.h"
#include "DatabaseManager.h"
#include "Logger.h"
#include <map>

namespace SentinelFS {

int BatchQueries::batchUpsertPeers(IStorageAPI* storage, const std::vector<PeerInfo>& peers) {
    if (peers.empty()) return 0;
    
    auto& logger = Logger::instance();
    if (logger.isDebugEnabled()) {
        logger.debug("Batch upserting " + std::to_string(peers.size()) + " peers", "BatchQueries");
    }
    
    // Get DatabaseManager from storage (assuming storage provides it)
    // This is a temporary solution - ideally storage should be injected with DatabaseManager
    void* dbPtr = storage->getDB();
    if (!dbPtr) return 0;
    
    // For now, we'll use the old way but with better error handling
    // In a full refactor, storage would accept DatabaseManager as dependency
    int successCount = 0;
    
    try {
        // Use storage's existing transaction handling if available
        for (const auto& peer : peers) {
            if (storage->addPeer(peer)) {
                successCount++;
            }
        }
    } catch (const std::exception& e) {
        logger.error("Batch upsert failed: " + std::string(e.what()), "BatchQueries");
        return successCount; // Return partial success
    }
    
    return successCount;
}

std::map<std::string, PeerInfo> BatchQueries::batchGetPeers(IStorageAPI* storage, const std::vector<std::string>& peerIds) {
    std::map<std::string, PeerInfo> results;
    
    auto& logger = Logger::instance();
    if (logger.isDebugEnabled()) {
        logger.debug("Batch getting " + std::to_string(peerIds.size()) + " peers", "BatchQueries");
    }
    
    try {
        for (const auto& peerId : peerIds) {
            auto peerInfo = storage->getPeer(peerId);
            if (peerInfo.has_value()) {
                results[peerId] = peerInfo.value();
            }
        }
    } catch (const std::exception& e) {
        logger.error("Batch get peers failed: " + std::string(e.what()), "BatchQueries");
    }
    
    return results;
}

bool BatchQueries::batchUpdateLatencies(IStorageAPI* storage, const std::map<std::string, int>& latencies) {
    auto& logger = Logger::instance();
    if (logger.isDebugEnabled()) {
        logger.debug("Batch updating " + std::to_string(latencies.size()) + " latencies", "BatchQueries");
    }
    
    bool allSuccess = true;
    
    try {
        for (const auto& [peerId, latency] : latencies) {
            if (!storage->updatePeerLatency(peerId, latency)) {
                allSuccess = false;
                logger.warn("Failed to update latency for peer: " + peerId, "BatchQueries");
            }
        }
    } catch (const std::exception& e) {
        logger.error("Batch update latencies failed: " + std::string(e.what()), "BatchQueries");
        return false;
    }
    
    return allSuccess;
}

int BatchQueries::batchInsertOperations(DatabaseManager* db, const std::vector<OperationInfo>& ops) {
    if (ops.empty()) return 0;
    
    auto& logger = Logger::instance();
    if (logger.isDebugEnabled()) {
        logger.debug("Batch inserting " + std::to_string(ops.size()) + " operations", "BatchQueries");
    }
    
    int successCount = 0;
    
    // Use DatabaseManager's transaction
    auto txn = db->beginTransaction();
    
    try {
        auto stmt = db->getInsertOperationStmt();
        
        for (const auto& op : ops) {
            stmt->clearBindings();
            stmt->bind(1, op.fileId);
            stmt->bind(2, op.deviceId);
            stmt->bind(3, static_cast<int>(op.opType));
            stmt->bind(4, static_cast<int>(op.status));
            stmt->bind(5, op.timestamp);
            
            if (stmt->step()) {
                successCount++;
            }
            stmt->reset();
        }
        
        txn->commit();
        logger.info("Successfully inserted " + std::to_string(successCount) + " operations", "BatchQueries");
        
    } catch (const std::exception& e) {
        logger.error("Batch insert failed: " + std::string(e.what()), "BatchQueries");
        // Transaction will auto-rollback on exception
    }
    
    return successCount;
}

std::vector<BatchQueries::PendingOperation> BatchQueries::getPendingOperations(DatabaseManager* db, int limit) {
    std::vector<PendingOperation> results;
    
    auto stmt = db->prepare("SELECT o.id, o.file_id, f.path, o.op_type, o.status, o.timestamp "
                           "FROM operations o "
                           "JOIN files f ON o.file_id = f.id "
                           "WHERE o.status = ? "
                           "ORDER BY o.timestamp ASC "
                           "LIMIT ?");
    
    stmt->bind(1, static_cast<int>(DBHelper::StatusType::PENDING));
    stmt->bind(2, limit);
    
    while (stmt->step()) {
        PendingOperation op;
        op.id = stmt->getColumnInt(0);
        op.fileId = stmt->getColumnInt(1);
        op.filePath = stmt->getColumnString(2);
        op.opType = static_cast<DBHelper::OpType>(stmt->getColumnInt(3));
        op.status = static_cast<DBHelper::StatusType>(stmt->getColumnInt(4));
        op.timestamp = stmt->getColumnInt64(5);
        
        results.push_back(op);
    }
    
    return results;
}

bool BatchQueries::updateOperationStatus(DatabaseManager* db, int operationId, DBHelper::StatusType newStatus) {
    auto stmt = db->prepare("UPDATE operations SET status = ? WHERE id = ?");
    
    stmt->bind(1, static_cast<int>(newStatus));
    stmt->bind(2, operationId);
    
    return stmt->step();
}

int BatchQueries::batchUpdateFileHashes(DatabaseManager* db, const std::vector<FileInfo>& files) {
    if (files.empty()) return 0;
    
    auto& logger = Logger::instance();
    int successCount = 0;
    
    auto txn = db->beginTransaction();
    
    try {
        auto stmt = db->prepare("UPDATE files SET hash = ?, size = ?, modified_time = ? WHERE path = ?");
        
        for (const auto& file : files) {
            stmt->clearBindings();
            stmt->bind(1, file.hash);
            stmt->bind(2, file.size);
            stmt->bind(3, file.modifiedTime);
            stmt->bind(4, file.path);
            
            if (stmt->step()) {
                successCount++;
            }
            stmt->reset();
        }
        
        txn->commit();
        logger.info("Updated " + std::to_string(successCount) + " file hashes", "BatchQueries");
        
    } catch (const std::exception& e) {
        logger.error("Batch update failed: " + std::string(e.what()), "BatchQueries");
    }
    
    return successCount;
}

std::vector<std::string> BatchQueries::getOrphanedFiles(DatabaseManager* db) {
    std::vector<std::string> orphanedPaths;
    
    // Files that exist but have no recent operations (older than 30 days)
    auto stmt = db->prepare("SELECT f.path FROM files f "
                           "LEFT JOIN operations o ON f.id = o.file_id AND o.timestamp > ? "
                           "WHERE o.id IS NULL AND f.modified_time < ?");
    
    int64_t thirtyDaysAgo = time(nullptr) - (30 * 24 * 3600);
    int64_t sixtyDaysAgo = time(nullptr) - (60 * 24 * 3600);
    
    stmt->bind(1, thirtyDaysAgo);
    stmt->bind(2, sixtyDaysAgo);
    
    while (stmt->step()) {
        orphanedPaths.push_back(stmt->getColumnString(0));
    }
    
    return orphanedPaths;
}

bool BatchQueries::cleanupOldRecords(DatabaseManager* db, int daysToKeep) {
    auto& logger = Logger::instance();
    
    int64_t cutoffTime = time(nullptr) - (daysToKeep * 24 * 3600);
    
    auto txn = db->beginTransaction();
    
    try {
        // Clean old operations
        auto stmt = db->prepare("DELETE FROM operations WHERE timestamp < ?");
        stmt->bind(1, cutoffTime);
        stmt->step();
        
        int deletedOps = db->changes();
        
        // Clean old files that have no operations
        stmt = db->prepare("DELETE FROM files WHERE id NOT IN (SELECT DISTINCT file_id FROM operations) AND modified_time < ?");
        stmt->bind(1, cutoffTime);
        stmt->step();
        
        int deletedFiles = db->changes();
        
        txn->commit();
        
        logger.info("Cleanup completed: deleted " + std::to_string(deletedOps) + 
                   " operations and " + std::to_string(deletedFiles) + " files", "BatchQueries");
        
        return true;
        
    } catch (const std::exception& e) {
        logger.error("Cleanup failed: " + std::string(e.what()), "BatchQueries");
        return false;
    }
}

} // namespace SentinelFS
