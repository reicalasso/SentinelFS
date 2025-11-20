#include "ConflictManager.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <chrono>

namespace SentinelFS {

bool ConflictManager::addConflict(const ConflictInfo& conflict) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::WARN, "Conflict detected for file: " + conflict.path + " with peer " + conflict.remotePeerId, "ConflictManager");
    
    const char* sql = "INSERT INTO conflicts (path, local_hash, remote_hash, remote_peer_id, "
                     "local_timestamp, remote_timestamp, local_size, remote_size, strategy, "
                     "resolved, detected_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "ConflictManager");
        metrics.incrementSyncErrors();
        return false;
    }

    sqlite3_bind_text(stmt, 1, conflict.path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, conflict.localHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, conflict.remoteHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, conflict.remotePeerId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, conflict.localTimestamp);
    sqlite3_bind_int64(stmt, 6, conflict.remoteTimestamp);
    sqlite3_bind_int64(stmt, 7, conflict.localSize);
    sqlite3_bind_int64(stmt, 8, conflict.remoteSize);
    sqlite3_bind_int(stmt, 9, conflict.strategy);
    sqlite3_bind_int(stmt, 10, conflict.resolved ? 1 : 0);
    sqlite3_bind_int64(stmt, 11, conflict.detectedAt);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "ConflictManager");
        sqlite3_finalize(stmt);
        metrics.incrementSyncErrors();
        return false;
    }

    sqlite3_finalize(stmt);
    metrics.incrementConflicts();
    logger.log(LogLevel::INFO, "Conflict recorded for: " + conflict.path, "ConflictManager");
    return true;
}

std::vector<ConflictInfo> ConflictManager::getUnresolvedConflicts() {
    const char* sql = "SELECT id, path, local_hash, remote_hash, remote_peer_id, "
                     "local_timestamp, remote_timestamp, local_size, remote_size, "
                     "strategy, resolved, detected_at, resolved_at "
                     "FROM conflicts WHERE resolved = 0 ORDER BY detected_at DESC;";
    return queryConflicts(sql);
}

std::vector<ConflictInfo> ConflictManager::getConflictsForFile(const std::string& path) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    const char* sql = "SELECT id, path, local_hash, remote_hash, remote_peer_id, "
                     "local_timestamp, remote_timestamp, local_size, remote_size, "
                     "strategy, resolved, detected_at, resolved_at "
                     "FROM conflicts WHERE path = ? ORDER BY detected_at DESC;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "ConflictManager");
        metrics.incrementSyncErrors();
        return {};
    }

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    
    std::vector<ConflictInfo> conflicts;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        conflicts.push_back(parseConflictRow(stmt));
    }

    sqlite3_finalize(stmt);
    return conflicts;
}

bool ConflictManager::markConflictResolved(int conflictId) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::INFO, "Marking conflict resolved: ID " + std::to_string(conflictId), "ConflictManager");
    
    const char* sql = "UPDATE conflicts SET resolved = 1, resolved_at = ? WHERE id = ?;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "ConflictManager");
        metrics.incrementSyncErrors();
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    sqlite3_bind_int64(stmt, 1, timestamp);
    sqlite3_bind_int(stmt, 2, conflictId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "ConflictManager");
        sqlite3_finalize(stmt);
        metrics.incrementSyncErrors();
        return false;
    }

    sqlite3_finalize(stmt);
    logger.log(LogLevel::INFO, "Conflict resolved successfully: ID " + std::to_string(conflictId), "ConflictManager");
    return true;
}

std::pair<int, int> ConflictManager::getConflictStats() {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    const char* sql = "SELECT COUNT(*) as total, "
                     "SUM(CASE WHEN resolved = 0 THEN 1 ELSE 0 END) as unresolved "
                     "FROM conflicts;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "ConflictManager");
        metrics.incrementSyncErrors();
        return {0, 0};
    }

    std::pair<int, int> stats = {0, 0};
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats.first = sqlite3_column_int(stmt, 0);   // total
        stats.second = sqlite3_column_int(stmt, 1);  // unresolved
    }

    sqlite3_finalize(stmt);
    return stats;
}

std::vector<ConflictInfo> ConflictManager::queryConflicts(const char* sql, sqlite3_stmt* preStmt) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    sqlite3_stmt* stmt = preStmt;
    sqlite3* db = handler_->getDB();
    
    if (!stmt && sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "ConflictManager");
        metrics.incrementSyncErrors();
        return {};
    }

    std::vector<ConflictInfo> conflicts;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        conflicts.push_back(parseConflictRow(stmt));
    }

    sqlite3_finalize(stmt);
    return conflicts;
}

ConflictInfo ConflictManager::parseConflictRow(sqlite3_stmt* stmt) {
    ConflictInfo info;
    info.id = sqlite3_column_int(stmt, 0);
    info.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    info.localHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    info.remoteHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    info.remotePeerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    info.localTimestamp = sqlite3_column_int64(stmt, 5);
    info.remoteTimestamp = sqlite3_column_int64(stmt, 6);
    info.localSize = sqlite3_column_int64(stmt, 7);
    info.remoteSize = sqlite3_column_int64(stmt, 8);
    info.strategy = sqlite3_column_int(stmt, 9);
    info.resolved = sqlite3_column_int(stmt, 10) == 1;
    info.detectedAt = sqlite3_column_int64(stmt, 11);
    info.resolvedAt = sqlite3_column_int64(stmt, 12);
    return info;
}

} // namespace SentinelFS
