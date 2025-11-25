#include "PeerManager.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <ctime>

namespace SentinelFS {

bool PeerManager::addPeer(const PeerInfo& peer) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    sqlite3* db = handler_->getDB();
    
    logger.log(LogLevel::DEBUG, "Adding peer: " + peer.id + " at " + peer.ip + ":" + std::to_string(peer.port), "PeerManager");
    
    // First, delete any existing peer with the same IP:port (different ID is a restart)
    const char* deleteSql = "DELETE FROM peers WHERE address = ? AND port = ? AND id != ?;";
    sqlite3_stmt* deleteStmt;
    if (sqlite3_prepare_v2(db, deleteSql, -1, &deleteStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(deleteStmt, 1, peer.ip.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(deleteStmt, 2, peer.port);
        sqlite3_bind_text(deleteStmt, 3, peer.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(deleteStmt);
        sqlite3_finalize(deleteStmt);
    }
    
    // Also clean up stale peers (not seen in 5 minutes)
    const char* cleanupSql = "DELETE FROM peers WHERE last_seen < ?;";
    sqlite3_stmt* cleanupStmt;
    if (sqlite3_prepare_v2(db, cleanupSql, -1, &cleanupStmt, nullptr) == SQLITE_OK) {
        long long cutoff = std::time(nullptr) - 300; // 5 minutes ago
        sqlite3_bind_int64(cleanupStmt, 1, cutoff);
        sqlite3_step(cleanupStmt);
        sqlite3_finalize(cleanupStmt);
    }
    
    const char* sql = "INSERT OR REPLACE INTO peers (id, address, port, last_seen, status, latency) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        metrics.incrementSyncErrors();
        return false;
    }

    sqlite3_bind_text(stmt, 1, peer.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, peer.ip.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, peer.port);
    sqlite3_bind_int64(stmt, 4, peer.lastSeen);
    sqlite3_bind_text(stmt, 5, peer.status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, peer.latency);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        sqlite3_finalize(stmt);
        metrics.incrementSyncErrors();
        return false;
    }

    sqlite3_finalize(stmt);
    logger.log(LogLevel::INFO, "Peer added successfully: " + peer.id, "PeerManager");
    return true;
}

std::optional<PeerInfo> PeerManager::getPeer(const std::string& peerId) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    const char* sql = "SELECT id, address, port, last_seen, status, latency FROM peers WHERE id = ?;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        metrics.incrementSyncErrors();
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);

    std::optional<PeerInfo> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        PeerInfo peer;
        peer.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        peer.ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        peer.port = sqlite3_column_int(stmt, 2);
        peer.lastSeen = sqlite3_column_int64(stmt, 3);
        const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        peer.status = status ? status : "unknown";
        peer.latency = sqlite3_column_int(stmt, 5);
        result = peer;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<PeerInfo> PeerManager::getAllPeers() {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    std::vector<PeerInfo> peers;
    const char* sql = "SELECT id, address, port, last_seen, status, latency FROM peers;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        metrics.incrementSyncErrors();
        return peers;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PeerInfo peer;
        peer.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        peer.ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        peer.port = sqlite3_column_int(stmt, 2);
        peer.lastSeen = sqlite3_column_int64(stmt, 3);
        const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        peer.status = status ? status : "unknown";
        peer.latency = sqlite3_column_int(stmt, 5);
        peers.push_back(peer);
    }

    sqlite3_finalize(stmt);
    return peers;
}

bool PeerManager::updatePeerLatency(const std::string& peerId, int latency) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Updating latency for peer " + peerId + ": " + std::to_string(latency) + "ms", "PeerManager");
    
    const char* sql = "UPDATE peers SET latency = ? WHERE id = ?;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    bool inTransaction = false;
    char* errMsg = nullptr;

    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to begin transaction: " + std::string(errMsg), "PeerManager");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        return false;
    }
    inTransaction = true;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        metrics.incrementSyncErrors();
        if (inTransaction) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
        return false;
    }

    sqlite3_bind_int(stmt, 1, latency);
    sqlite3_bind_text(stmt, 2, peerId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        sqlite3_finalize(stmt);
        metrics.incrementSyncErrors();
        if (inTransaction) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
        return false;
    }

    sqlite3_finalize(stmt);

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to commit transaction: " + std::string(errMsg), "PeerManager");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}

std::vector<PeerInfo> PeerManager::getPeersByLatency() {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    std::vector<PeerInfo> peers;
    const char* sql = "SELECT id, address, port, last_seen, status, latency FROM peers "
                     "ORDER BY CASE WHEN latency = -1 THEN 999999 ELSE latency END ASC;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        metrics.incrementSyncErrors();
        return peers;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PeerInfo peer;
        peer.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        peer.ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        peer.port = sqlite3_column_int(stmt, 2);
        peer.lastSeen = sqlite3_column_int64(stmt, 3);
        const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        peer.status = status ? status : "unknown";
        peer.latency = sqlite3_column_int(stmt, 5);
        peers.push_back(peer);
    }

    sqlite3_finalize(stmt);
    return peers;
}

bool PeerManager::removePeer(const std::string& peerId) {
    auto& logger = Logger::instance();
    sqlite3* db = handler_->getDB();
    
    logger.log(LogLevel::INFO, "Removing peer: " + peerId, "PeerManager");
    
    const char* sql = "DELETE FROM peers WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if (success) {
        logger.log(LogLevel::INFO, "Peer removed successfully: " + peerId, "PeerManager");
    }
    
    return success;
}

} // namespace SentinelFS
