#include "PeerManager.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>

namespace SentinelFS {

bool PeerManager::addPeer(const PeerInfo& peer) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Adding peer: " + peer.id + " at " + peer.ip + ":" + std::to_string(peer.port), "PeerManager");
    
    const char* sql = "INSERT OR REPLACE INTO peers (id, address, port, last_seen, status, latency) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

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

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        metrics.incrementSyncErrors();
        return false;
    }

    sqlite3_bind_int(stmt, 1, latency);
    sqlite3_bind_text(stmt, 2, peerId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "PeerManager");
        sqlite3_finalize(stmt);
        metrics.incrementSyncErrors();
        return false;
    }

    sqlite3_finalize(stmt);
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

} // namespace SentinelFS
