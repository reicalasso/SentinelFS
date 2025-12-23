/**
 * @file BatchQueries.cpp
 * @brief Implementation of batch database operations
 */

#include "BatchQueries.h"
#include "Logger.h"
#include <map>
#include <sqlite3.h>

namespace SentinelFS {

int BatchQueries::batchUpsertPeers(IStorageAPI* storage, const std::vector<PeerInfo>& peers) {
    if (peers.empty()) return 0;
    
    auto& logger = Logger::instance();
    if (logger.isDebugEnabled()) {
        logger.debug("Batch upserting " + std::to_string(peers.size()) + " peers", "BatchQueries");
    }
    
    int successCount = 0;
    
    // Use transaction for batch insert
    void* dbPtr = storage->getDB();
    if (!dbPtr) return 0;
    sqlite3* db = static_cast<sqlite3*>(dbPtr);
    if (!db) return 0;
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            logger.error("Failed to begin transaction: " + std::string(errMsg), "BatchQueries");
            sqlite3_free(errMsg);
        }
        return 0;
    }
    
    for (const auto& peer : peers) {
        if (storage->addPeer(peer)) {
            successCount++;
        }
    }
    
    if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            logger.error("Failed to commit transaction: " + std::string(errMsg), "BatchQueries");
            sqlite3_free(errMsg);
        }
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        return 0;
    }
    
    if (logger.isDebugEnabled()) {
        logger.debug("Batch upsert completed: " + std::to_string(successCount) + 
                    "/" + std::to_string(peers.size()) + " successful", "BatchQueries");
    }
    
    return successCount;
}

std::map<std::string, PeerInfo> BatchQueries::batchGetPeers(
    IStorageAPI* storage, 
    const std::vector<std::string>& peerIds) 
{
    std::map<std::string, PeerInfo> result;
    
    if (peerIds.empty()) return result;
    
    // Build IN clause for single query
    void* dbPtr = storage->getDB();
    if (!dbPtr) return result;
    sqlite3* db = static_cast<sqlite3*>(dbPtr);
    if (!db) return result;
    
    std::string placeholders;
    for (size_t i = 0; i < peerIds.size(); ++i) {
        if (i > 0) placeholders += ",";
        placeholders += "?";
    }
    
    std::string sql = "SELECT id, ip, port, status, last_seen, latency FROM peers WHERE id IN (" + 
                      placeholders + ")";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }
    
    // Bind parameters
    for (size_t i = 0; i < peerIds.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), peerIds[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    
    // Fetch results
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PeerInfo peer;
        const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        
        peer.id = id && strlen(id) > 0 ? id : "";
        peer.ip = ip && strlen(ip) > 0 ? ip : "";
        peer.port = sqlite3_column_int(stmt, 2);
        peer.status = status && strlen(status) > 0 ? status : "";
        peer.lastSeen = sqlite3_column_int64(stmt, 4);
        peer.latency = sqlite3_column_int(stmt, 5);
        
        if (!peer.id.empty()) {
            result[peer.id] = peer;
        }
    }
    
    sqlite3_finalize(stmt);
    
    return result;
}

bool BatchQueries::executeInTransaction(
    IStorageAPI* storage,
    std::function<bool()> operations)
{
    void* dbPtr = storage->getDB();
    if (!dbPtr) return false;
    sqlite3* db = static_cast<sqlite3*>(dbPtr);
    if (!db) return false;
    
    // Start transaction
    if (sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr) != SQLITE_OK) {
        return false;
    }
    
    // Execute operations
    bool success = operations();
    
    // Commit or rollback
    if (success) {
        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    
    return success;
}

void BatchQueries::batchUpdateLatencies(
    IStorageAPI* storage,
    const std::map<std::string, int>& latencies)
{
    if (latencies.empty()) return;
    
    auto& logger = Logger::instance();
    void* dbPtr = storage->getDB();
    if (!dbPtr) return;
    sqlite3* db = static_cast<sqlite3*>(dbPtr);
    if (!db) return;
    
    // Batch update with transaction
    char* errMsg = nullptr;
    if (sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            logger.error("Failed to begin transaction: " + std::string(errMsg), "BatchQueries");
            sqlite3_free(errMsg);
        }
        return;
    }
    
    const char* sql = "UPDATE peers SET latency = ?, last_seen = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    bool success = true;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& [peerId, latency] : latencies) {
            long long now = std::time(nullptr);
            
            sqlite3_bind_int(stmt, 1, latency);
            sqlite3_bind_int64(stmt, 2, now);
            sqlite3_bind_text(stmt, 3, peerId.c_str(), -1, SQLITE_TRANSIENT);
            
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                success = false;
                logger.warn("Failed to update latency for peer: " + peerId, "BatchQueries");
            }
            sqlite3_reset(stmt);
        }
        
        sqlite3_finalize(stmt);
    } else {
        success = false;
    }
    
    if (success) {
        if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            if (errMsg) {
                logger.error("Failed to commit transaction: " + std::string(errMsg), "BatchQueries");
                sqlite3_free(errMsg);
            }
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    } else {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    
    if (logger.isDebugEnabled()) {
        logger.debug("Batch updated " + std::to_string(latencies.size()) + " peer latencies", "BatchQueries");
    }
}

} // namespace SentinelFS
