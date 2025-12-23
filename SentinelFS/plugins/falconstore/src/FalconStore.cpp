/**
 * @file FalconStore.cpp
 * @brief FalconStore plugin implementation
 */

#include "FalconStore.h"
#include "QueryBuilder.h"
#include "MigrationManager.h"
#include "Cache.h"
#include "Logger.h"
#include "EventBus.h"
#include "../../../core/include/DatabaseManager.h"

#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <atomic>

namespace SentinelFS {

// ============================================================================
// Implementation
// ============================================================================

struct FalconStore::Impl {
    // Configuration
    Falcon::FalconConfig config;
    
    // Database
    sqlite3* db{nullptr};
    std::string dbPath;
    DatabaseManager* dbManager{nullptr};
    
    // Components
    std::unique_ptr<Falcon::MigrationManager> migrationManager;
    std::unique_ptr<Falcon::LRUCache> cache;
    
    // Event bus
    EventBus* eventBus{nullptr};
    
    // Statistics
    mutable std::mutex statsMutex;
    Falcon::FalconStats stats;
    
    // Thread safety
    mutable std::shared_mutex dbMutex;
    
    // Query timing
    std::chrono::steady_clock::time_point lastQueryStart;
    
    void updateQueryStats(double durationMs) {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.totalQueries++;
        
        // Update average
        double total = stats.avgQueryTimeMs * (stats.totalQueries - 1) + durationMs;
        stats.avgQueryTimeMs = total / stats.totalQueries;
        
        // Update max
        if (durationMs > stats.maxQueryTimeMs) {
            stats.maxQueryTimeMs = durationMs;
        }
        
        // Count slow queries
        if (durationMs > 100.0) {
            stats.slowQueries++;
        }
    }
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

FalconStore::FalconStore() : impl_(std::make_unique<Impl>()) {
    // Set default config - use same path as legacy storage for compatibility
    const char* envPath = std::getenv("SENTINEL_DB_PATH");
    const char* home = std::getenv("HOME");
    
    if (envPath) {
        impl_->dbPath = envPath;
    } else if (home) {
        impl_->dbPath = std::string(home) + "/.local/share/sentinelfs/sentinel.db";
    } else {
        impl_->dbPath = "/tmp/sentinel.db";
    }
    impl_->config.dbPath = impl_->dbPath;
}

FalconStore::~FalconStore() {
    shutdown();
}

// ============================================================================
// IPlugin Interface
// ============================================================================

bool FalconStore::initialize(EventBus* eventBus) {
    auto& logger = Logger::instance();
    impl_->eventBus = eventBus;
    
    logger.log(LogLevel::INFO, "Initializing FalconStore v" + getVersion(), "FalconStore");
    logger.log(LogLevel::INFO, "Database path: " + impl_->dbPath, "FalconStore");
    
    // Ensure directory exists
    std::filesystem::path dbDir = std::filesystem::path(impl_->dbPath).parent_path();
    std::filesystem::create_directories(dbDir);
    
    // Open database
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(impl_->dbPath.c_str(), &impl_->db, flags, nullptr);
    
    if (rc != SQLITE_OK) {
        const char* err_msg = impl_->db ? sqlite3_errmsg(impl_->db) : "Failed to allocate database";
        logger.log(LogLevel::ERROR, "Failed to open database: " + std::string(err_msg), "FalconStore");
        if (impl_->db) {
            sqlite3_close(impl_->db);
            impl_->db = nullptr;
        }
        return false;
    }
    
    // Configure SQLite
    if (impl_->config.enableWAL) {
        sqlite3_exec(impl_->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    }
    
    if (impl_->config.enableForeignKeys) {
        sqlite3_exec(impl_->db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    }
    
    sqlite3_busy_timeout(impl_->db, impl_->config.busyTimeout);
    
    if (!impl_->config.synchronous) {
        sqlite3_exec(impl_->db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    }
    
    // Initialize migration manager
    impl_->migrationManager = std::make_unique<Falcon::MigrationManager>(impl_->db);
    impl_->migrationManager->registerDefaultMigrations();
    
    // Run migrations
    if (!impl_->migrationManager->migrateUp()) {
        logger.log(LogLevel::ERROR, "Migration failed", "FalconStore");
        return false;
    }
    
    impl_->stats.schemaVersion = impl_->migrationManager->getCurrentVersion();
    logger.log(LogLevel::INFO, "Schema version: " + std::to_string(impl_->stats.schemaVersion), "FalconStore");
    
    // Initialize cache
    if (impl_->config.enableCache) {
        impl_->cache = std::make_unique<Falcon::LRUCache>(
            impl_->config.cacheMaxSize,
            impl_->config.cacheMaxMemory,
            impl_->config.cacheTTL
        );
    }
    
    // Get database size
    try {
        impl_->stats.dbSizeBytes = std::filesystem::file_size(impl_->dbPath);
    } catch (...) {}
    
    logger.log(LogLevel::INFO, "FalconStore initialized successfully", "FalconStore");
    return true;
}

void FalconStore::shutdown() {
    auto& logger = Logger::instance();
    
    if (impl_->db) {
        logger.log(LogLevel::INFO, "Shutting down FalconStore", "FalconStore");
        
        // Checkpoint WAL
        sqlite3_wal_checkpoint_v2(impl_->db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
        
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
    }
    
    impl_->cache.reset();
    impl_->migrationManager.reset();
}

// ============================================================================
// Configuration
// ============================================================================

void FalconStore::configure(const Falcon::FalconConfig& config) {
    impl_->config = config;
    if (!config.dbPath.empty()) {
        impl_->dbPath = config.dbPath;
    }
}

// ============================================================================
// IStorageAPI - File Operations
// ============================================================================

bool FalconStore::addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    // Check if file exists to determine if this is INSERT or UPDATE
    bool isUpdate = false;
    {
        const char* checkSql = "SELECT 1 FROM files WHERE path = ? LIMIT 1";
        sqlite3_stmt* checkStmt;
        if (sqlite3_prepare_v2(impl_->db, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(checkStmt, 1, path.c_str(), -1, SQLITE_STATIC);
            isUpdate = (sqlite3_step(checkStmt) == SQLITE_ROW);
            sqlite3_finalize(checkStmt);
        }
    }
    
    const char* sql = R"(
        INSERT INTO files (path, hash, size, modified, synced, version)
        VALUES (?, ?, ?, ?, 0, 1)
        ON CONFLICT(path) DO UPDATE SET
            hash = excluded.hash,
            size = excluded.size,
            modified = excluded.modified,
            version = version + 1
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, size);
    sqlite3_bind_int64(stmt, 4, timestamp);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    // Update cache with new value (instead of just invalidating)
    if (impl_->cache && success) {
        std::string cacheKey = "file:" + path;
        std::string cacheData = path + "|" + hash + "|" + 
                                std::to_string(timestamp) + "|" + 
                                std::to_string(size);
        impl_->cache->put(cacheKey, cacheData);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        if (isUpdate) {
            impl_->stats.updateQueries++;
        } else {
            impl_->stats.insertQueries++;
        }
    }
    
    return success;
}

bool FalconStore::removeFile(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "DELETE FROM files WHERE path = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    // Invalidate cache
    if (impl_->cache && success) {
        impl_->cache->invalidate("file:" + path);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries++;
    }
    
    return success;
}

std::optional<FileMetadata> FalconStore::getFile(const std::string& path) {
    std::string cacheKey = "file:" + path;
    
    // Check cache first
    if (impl_->cache) {
        auto cached = impl_->cache->get(cacheKey);
        if (cached) {
            // Cache hit - deserialize
            // Format: "path|hash|timestamp|size"
            const std::string& data = *cached;
            size_t p1 = data.find('|');
            size_t p2 = data.find('|', p1 + 1);
            size_t p3 = data.find('|', p2 + 1);
            
            if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
                FileMetadata file;
                file.path = data.substr(0, p1);
                file.hash = data.substr(p1 + 1, p2 - p1 - 1);
                file.timestamp = std::stoll(data.substr(p2 + 1, p3 - p2 - 1));
                file.size = std::stoll(data.substr(p3 + 1));
                return file;
            }
        }
    }
    
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "SELECT path, hash, modified, size FROM files WHERE path = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    
    std::optional<FileMetadata> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        FileMetadata file;
        const char* filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        file.path = filePath && strlen(filePath) > 0 ? filePath : "";
        file.hash = hash && strlen(hash) > 0 ? hash : "";
        file.timestamp = sqlite3_column_int64(stmt, 2);
        file.size = sqlite3_column_int64(stmt, 3);
        result = file;
        
        // Store in cache
        if (impl_->cache && !file.path.empty()) {
            std::string cacheData = file.path + "|" + file.hash + "|" + 
                                    std::to_string(file.timestamp) + "|" + 
                                    std::to_string(file.size);
            impl_->cache->put(cacheKey, cacheData);
        }
    }
    
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return result;
}

// ============================================================================
// IStorageAPI - Peer Operations
// ============================================================================

bool FalconStore::addPeer(const PeerInfo& peer) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    // Map status string to status_id
    int statusId = 1;  // default: active
    if (peer.status == "offline") statusId = 6;
    else if (peer.status == "pending") statusId = 2;
    
    // First check if peer exists by address:port (peer IDs change on restart)
    const char* checkSql = "SELECT id, peer_id FROM peers WHERE address = ? AND port = ?";
    sqlite3_stmt* checkStmt;
    bool existsByAddress = false;
    int existingId = 0;
    
    if (sqlite3_prepare_v2(impl_->db, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(checkStmt, 1, peer.ip.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(checkStmt, 2, peer.port);
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            existsByAddress = true;
            existingId = sqlite3_column_int(checkStmt, 0);
        }
        sqlite3_finalize(checkStmt);
    }
    
    bool success = false;
    
    if (existsByAddress) {
        // Update existing peer by address:port (update peer_id since it changes on restart)
        const char* updateSql = R"(
            UPDATE peers SET peer_id = ?, name = ?, status_id = ?, last_seen = ?, latency = ?
            WHERE id = ?
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(impl_->db, updateSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, peer.id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, peer.id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, statusId);
            sqlite3_bind_int64(stmt, 4, peer.lastSeen);
            sqlite3_bind_int(stmt, 5, peer.latency);
            sqlite3_bind_int(stmt, 6, existingId);
            success = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
        }
    } else {
        // Insert new peer
        const char* sql = R"(
            INSERT INTO peers (peer_id, name, address, port, status_id, last_seen, latency)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(peer_id) DO UPDATE SET
                name = excluded.name,
                address = excluded.address,
                port = excluded.port,
                status_id = excluded.status_id,
                last_seen = excluded.last_seen,
                latency = excluded.latency
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, peer.id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, peer.id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, peer.ip.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 4, peer.port);
            sqlite3_bind_int(stmt, 5, statusId);
            sqlite3_bind_int64(stmt, 6, peer.lastSeen);
            sqlite3_bind_int(stmt, 7, peer.latency);
            success = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
        }
    }
    
    return success;
}

bool FalconStore::removePeer(const std::string& peerId) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    const char* sql = "DELETE FROM peers WHERE peer_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    return success;
}

std::optional<PeerInfo> FalconStore::getPeer(const std::string& peerId) {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    const char* sql = "SELECT peer_id, address, port, status_id, last_seen, latency FROM peers WHERE peer_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    
    sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
    
    std::optional<PeerInfo> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        PeerInfo peer;
        const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        peer.id = id && strlen(id) > 0 ? id : "";
        peer.ip = ip && strlen(ip) > 0 ? ip : "";
        peer.port = sqlite3_column_int(stmt, 2);
        int statusId = sqlite3_column_int(stmt, 3);
        peer.status = (statusId == 6) ? "offline" : "active";
        peer.lastSeen = sqlite3_column_int64(stmt, 4);
        peer.latency = sqlite3_column_int(stmt, 5);
        result = peer;
    }
    
    sqlite3_finalize(stmt);
    return result;
}

std::vector<PeerInfo> FalconStore::getAllPeers() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    std::vector<PeerInfo> peers;
    const char* sql = "SELECT peer_id, address, port, status_id, last_seen, latency FROM peers";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PeerInfo peer;
            const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            peer.id = id && strlen(id) > 0 ? id : "";
            peer.ip = ip && strlen(ip) > 0 ? ip : "";
            peer.port = sqlite3_column_int(stmt, 2);
            int statusId = sqlite3_column_int(stmt, 3);
            peer.status = (statusId == 6) ? "offline" : "active";
            peer.lastSeen = sqlite3_column_int64(stmt, 4);
            peer.latency = sqlite3_column_int(stmt, 5);
            
            if (!peer.id.empty()) {
                peers.push_back(peer);
            }
        }
        sqlite3_finalize(stmt);
    }
    
    return peers;
}

bool FalconStore::updatePeerLatency(const std::string& peerId, int latency) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    const char* sql = "UPDATE peers SET latency = ? WHERE peer_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, latency);
    sqlite3_bind_text(stmt, 2, peerId.c_str(), -1, SQLITE_STATIC);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    return success;
}

std::vector<PeerInfo> FalconStore::getPeersByLatency() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    std::vector<PeerInfo> peers;
    const char* sql = "SELECT peer_id, address, port, status_id, last_seen, latency FROM peers ORDER BY CASE WHEN latency < 0 THEN 999999 ELSE latency END ASC";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PeerInfo peer;
            const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            peer.id = id && strlen(id) > 0 ? id : "";
            peer.ip = ip && strlen(ip) > 0 ? ip : "";
            peer.port = sqlite3_column_int(stmt, 2);
            int statusId = sqlite3_column_int(stmt, 3);
            peer.status = (statusId == 6) ? "offline" : "active";
            peer.lastSeen = sqlite3_column_int64(stmt, 4);
            peer.latency = sqlite3_column_int(stmt, 5);
            
            if (!peer.id.empty()) {
                peers.push_back(peer);
            }
        }
        sqlite3_finalize(stmt);
    }
    
    return peers;
}

// ============================================================================
// IStorageAPI - Conflict Operations
// ============================================================================

bool FalconStore::addConflict(const ConflictInfo& conflict) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    // First get file_id from path
    const char* getFileSql = "SELECT id FROM files WHERE path = ?";
    sqlite3_stmt* getStmt;
    int fileId = 0;
    
    if (sqlite3_prepare_v2(impl_->db, getFileSql, -1, &getStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(getStmt, 1, conflict.path.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(getStmt) == SQLITE_ROW) {
            fileId = sqlite3_column_int(getStmt, 0);
        }
        sqlite3_finalize(getStmt);
    }
    
    if (fileId == 0) return false;
    
    const char* sql = R"(
        INSERT INTO conflicts (file_id, local_hash, remote_hash, local_size, remote_size,
                               local_timestamp, remote_timestamp, remote_peer_id, strategy)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, fileId);
    sqlite3_bind_text(stmt, 2, conflict.localHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, conflict.remoteHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, conflict.localSize);
    sqlite3_bind_int64(stmt, 5, conflict.remoteSize);
    sqlite3_bind_int64(stmt, 6, conflict.localTimestamp);
    sqlite3_bind_int64(stmt, 7, conflict.remoteTimestamp);
    sqlite3_bind_text(stmt, 8, conflict.remotePeerId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 9, conflict.strategy);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    return success;
}

bool FalconStore::markConflictResolved(int conflictId, int strategy) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "UPDATE conflicts SET resolved = 1, resolved_at = strftime('%s', 'now'), strategy = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, strategy);
    sqlite3_bind_int(stmt, 2, conflictId);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.updateQueries++;
    }
    
    return success;
}

std::vector<ConflictInfo> FalconStore::getUnresolvedConflicts() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    std::vector<ConflictInfo> conflicts;
    const char* sql = R"(
        SELECT c.id, f.path, c.local_hash, c.remote_hash, c.remote_peer_id,
               c.local_timestamp, c.remote_timestamp, c.local_size, c.remote_size,
               c.strategy, c.resolved, c.detected_at
        FROM conflicts c
        JOIN files f ON c.file_id = f.id
        WHERE c.resolved = 0
        ORDER BY c.detected_at DESC
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ConflictInfo conflict;
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* localHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* remoteHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* remotePeerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            
            conflict.id = sqlite3_column_int(stmt, 0);
            conflict.path = path && strlen(path) > 0 ? path : "";
            conflict.localHash = localHash && strlen(localHash) > 0 ? localHash : "";
            conflict.remoteHash = remoteHash && strlen(remoteHash) > 0 ? remoteHash : "";
            conflict.remotePeerId = remotePeerId && strlen(remotePeerId) > 0 ? remotePeerId : "";
            conflict.localTimestamp = sqlite3_column_int64(stmt, 5);
            conflict.remoteTimestamp = sqlite3_column_int64(stmt, 6);
            conflict.localSize = sqlite3_column_int64(stmt, 7);
            conflict.remoteSize = sqlite3_column_int64(stmt, 8);
            conflict.strategy = sqlite3_column_int(stmt, 9);
            conflict.resolved = sqlite3_column_int(stmt, 10) != 0;
            conflict.detectedAt = sqlite3_column_int64(stmt, 11);
            conflicts.push_back(conflict);
        }
        sqlite3_finalize(stmt);
    }
    
    return conflicts;
}

std::vector<ConflictInfo> FalconStore::getConflictsForFile(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    std::vector<ConflictInfo> conflicts;
    const char* sql = R"(
        SELECT c.id, f.path, c.local_hash, c.remote_hash, c.remote_peer_id,
               c.local_timestamp, c.remote_timestamp, c.local_size, c.remote_size,
               c.strategy, c.resolved, c.detected_at
        FROM conflicts c
        JOIN files f ON c.file_id = f.id
        WHERE f.path = ?
        ORDER BY c.detected_at DESC
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ConflictInfo conflict;
            const char* filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* localHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* remoteHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* remotePeerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            
            conflict.id = sqlite3_column_int(stmt, 0);
            conflict.path = filePath && strlen(filePath) > 0 ? filePath : "";
            conflict.localHash = localHash && strlen(localHash) > 0 ? localHash : "";
            conflict.remoteHash = remoteHash && strlen(remoteHash) > 0 ? remoteHash : "";
            conflict.remotePeerId = remotePeerId && strlen(remotePeerId) > 0 ? remotePeerId : "";
            conflict.localTimestamp = sqlite3_column_int64(stmt, 5);
            conflict.remoteTimestamp = sqlite3_column_int64(stmt, 6);
            conflict.localSize = sqlite3_column_int64(stmt, 7);
            conflict.remoteSize = sqlite3_column_int64(stmt, 8);
            conflict.strategy = sqlite3_column_int(stmt, 9);
            conflict.resolved = sqlite3_column_int(stmt, 10) != 0;
            conflict.detectedAt = sqlite3_column_int64(stmt, 11);
            conflicts.push_back(conflict);
        }
        sqlite3_finalize(stmt);
    }
    
    return conflicts;
}

std::pair<int, int> FalconStore::getConflictStats() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    int total = 0, unresolved = 0;
    
    const char* sql = "SELECT COUNT(*), SUM(CASE WHEN resolved = 0 THEN 1 ELSE 0 END) FROM conflicts";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total = sqlite3_column_int(stmt, 0);
            unresolved = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
    
    return {total, unresolved};
}

// ============================================================================
// IStorageAPI - Sync Queue / Access Log
// ============================================================================

bool FalconStore::enqueueSyncOperation(const std::string& filePath, const std::string& opType, const std::string& status) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    // Get file_id
    const char* getFileSql = "SELECT id FROM files WHERE path = ?";
    sqlite3_stmt* getStmt;
    int fileId = 0;
    
    if (sqlite3_prepare_v2(impl_->db, getFileSql, -1, &getStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(getStmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(getStmt) == SQLITE_ROW) {
            fileId = sqlite3_column_int(getStmt, 0);
        }
        sqlite3_finalize(getStmt);
    }
    
    if (fileId == 0) return false;
    
    // Map opType to op_type_id
    int opTypeId = 1;  // default: create
    if (opType == "update") opTypeId = 2;
    else if (opType == "delete") opTypeId = 3;
    
    // Map status to status_id
    int statusId = 2;  // default: pending
    if (status == "active") statusId = 1;
    else if (status == "syncing") statusId = 3;
    else if (status == "completed") statusId = 4;
    else if (status == "failed") statusId = 5;
    
    const char* sql = R"(
        INSERT INTO sync_queue (file_id, peer_id, op_type_id, status_id)
        VALUES (?, '', ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, fileId);
    sqlite3_bind_int(stmt, 2, opTypeId);
    sqlite3_bind_int(stmt, 3, statusId);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    return success;
}

bool FalconStore::logFileAccess(const std::string& filePath, const std::string& opType, const std::string& deviceId, long long timestamp) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    // Get file_id (optional - may not exist)
    const char* getFileSql = "SELECT id FROM files WHERE path = ?";
    sqlite3_stmt* getStmt;
    int fileId = 0;
    
    if (sqlite3_prepare_v2(impl_->db, getFileSql, -1, &getStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(getStmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(getStmt) == SQLITE_ROW) {
            fileId = sqlite3_column_int(getStmt, 0);
        }
        sqlite3_finalize(getStmt);
    }
    
    // Map opType to op_type_id
    int opTypeId = 4;  // default: read
    if (opType == "write") opTypeId = 5;
    else if (opType == "create") opTypeId = 1;
    else if (opType == "update") opTypeId = 2;
    else if (opType == "delete") opTypeId = 3;
    
    const char* sql = R"(
        INSERT INTO activity_log (file_id, op_type_id, timestamp, details, peer_id)
        VALUES (?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    if (fileId > 0) {
        sqlite3_bind_int(stmt, 1, fileId);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    sqlite3_bind_int(stmt, 2, opTypeId);
    sqlite3_bind_int64(stmt, 3, timestamp);
    sqlite3_bind_text(stmt, 4, filePath.c_str(), -1, SQLITE_STATIC);  // Store path in details
    sqlite3_bind_text(stmt, 5, deviceId.c_str(), -1, SQLITE_STATIC);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    return success;
}

// ============================================================================
// Raw Database Access
// ============================================================================

void* FalconStore::getDB() {
    return impl_->db;
}

DatabaseManager* FalconStore::getDatabaseManager() {
    return impl_->dbManager;
}

void FalconStore::setDatabaseManager(DatabaseManager* dbManager) {
    impl_->dbManager = dbManager;
}

// ============================================================================
// FalconStore Specific API
// ============================================================================

std::unique_ptr<Falcon::QueryBuilder> FalconStore::query() {
    return std::make_unique<Falcon::SQLiteQueryBuilder>(impl_->db);
}

std::unique_ptr<Falcon::Transaction> FalconStore::beginTransaction() {
    // Start transaction (SQLite handles isolation, we just need to ensure BEGIN is atomic)
    // Note: The transaction will manage its own commit/rollback
    return std::make_unique<Falcon::SQLiteTransaction>(impl_->db);
}

bool FalconStore::execute(const std::string& sql) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    char* errMsg = nullptr;
    bool success = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &errMsg) == SQLITE_OK;
    if (errMsg) {
        Logger::instance().log(LogLevel::ERROR, "SQL error: " + std::string(errMsg), "FalconStore");
        sqlite3_free(errMsg);
    }
    return success;
}

bool FalconStore::execute(const std::string& sql, const std::vector<Falcon::QueryValue>& params) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    // Bind parameters
    int idx = 1;
    for (const auto& param : params) {
        std::visit([stmt, idx](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                sqlite3_bind_null(stmt, idx);
            } else if constexpr (std::is_same_v<T, bool>) {
                sqlite3_bind_int(stmt, idx, arg ? 1 : 0);
            } else if constexpr (std::is_same_v<T, int>) {
                sqlite3_bind_int(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                sqlite3_bind_int64(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, double>) {
                sqlite3_bind_double(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                sqlite3_bind_text(stmt, idx, arg.c_str(), -1, SQLITE_STATIC);
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                sqlite3_bind_blob(stmt, idx, arg.data(), arg.size(), SQLITE_STATIC);
            }
        }, param);
        idx++;
    }
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    return success;
}

Falcon::IMigrationManager* FalconStore::getMigrationManager() {
    return impl_->migrationManager.get();
}

Falcon::ICache* FalconStore::getCache() {
    return impl_->cache.get();
}

Falcon::FalconStats FalconStore::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->statsMutex);
    Falcon::FalconStats stats = impl_->stats;
    
    if (impl_->cache) {
        stats.cache = impl_->cache->getStats();
    }
    
    // Update DB size
    try {
        stats.dbSizeBytes = std::filesystem::file_size(impl_->dbPath);
    } catch (...) {}
    
    return stats;
}

void FalconStore::optimize() {
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Optimizing database...", "FalconStore");
    
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    
    // WAL checkpoint first - this is safe and reclaims WAL file space
    int walPages = 0, checkpointedPages = 0;
    int rc = sqlite3_wal_checkpoint_v2(impl_->db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, 
                                        &walPages, &checkpointedPages);
    if (rc == SQLITE_OK) {
        logger.log(LogLevel::DEBUG, "WAL checkpoint: " + std::to_string(checkpointedPages) + 
                   " pages checkpointed", "FalconStore");
    } else if (rc == SQLITE_BUSY) {
        // Busy is OK - means other connections are active, checkpoint what we can
        logger.log(LogLevel::DEBUG, "WAL checkpoint partial (database busy)", "FalconStore");
    }
    
    // ANALYZE updates query planner statistics - safe in WAL mode
    sqlite3_exec(impl_->db, "ANALYZE;", nullptr, nullptr, nullptr);
    
    // VACUUM is problematic in WAL mode - it temporarily switches to DELETE journal mode
    // which can cause issues with concurrent connections. Only run if explicitly needed.
    // For routine optimization, WAL checkpoint + ANALYZE is sufficient.
    // If VACUUM is truly needed, consider: PRAGMA wal_checkpoint(TRUNCATE); VACUUM; PRAGMA journal_mode=WAL;
    
    logger.log(LogLevel::INFO, "Database optimization complete", "FalconStore");
}

bool FalconStore::backup(const std::string& path) {
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Backing up database to: " + path, "FalconStore");
    
    sqlite3* backupDb;
    if (sqlite3_open(path.c_str(), &backupDb) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_backup* backup = sqlite3_backup_init(backupDb, "main", impl_->db, "main");
    if (!backup) {
        sqlite3_close(backupDb);
        return false;
    }
    
    sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(backupDb);
    
    logger.log(LogLevel::INFO, "Backup complete", "FalconStore");
    return true;
}

bool FalconStore::restore(const std::string& path) {
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Restoring database from: " + path, "FalconStore");
    
    if (!std::filesystem::exists(path)) {
        logger.log(LogLevel::ERROR, "Backup file not found: " + path, "FalconStore");
        return false;
    }
    
    sqlite3* backupDb;
    if (sqlite3_open(path.c_str(), &backupDb) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_backup* backup = sqlite3_backup_init(impl_->db, "main", backupDb, "main");
    if (!backup) {
        sqlite3_close(backupDb);
        return false;
    }
    
    sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(backupDb);
    
    logger.log(LogLevel::INFO, "Restore complete", "FalconStore");
    return true;
}

// ============================================================================
// Watched Folder Operations
// ============================================================================

bool FalconStore::addWatchedFolder(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = R"(
        INSERT INTO watched_folders (path, status_id) VALUES (?, 1)
        ON CONFLICT(path) DO UPDATE SET status_id = 1
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.insertQueries++;
    }
    
    return success;
}

bool FalconStore::removeWatchedFolder(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "DELETE FROM watched_folders WHERE path = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries++;
    }
    
    return success;
}

std::vector<WatchedFolder> FalconStore::getWatchedFolders() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::vector<WatchedFolder> folders;
    const char* sql = "SELECT id, path, added_at, status_id FROM watched_folders WHERE status_id = 1";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            WatchedFolder folder;
            folder.id = sqlite3_column_int(stmt, 0);
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            folder.path = path && strlen(path) > 0 ? path : "";
            folder.addedAt = sqlite3_column_int64(stmt, 2);
            folder.statusId = sqlite3_column_int(stmt, 3);
            folders.push_back(folder);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return folders;
}

bool FalconStore::isWatchedFolder(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "SELECT 1 FROM watched_folders WHERE path = ? AND status_id = 1 LIMIT 1";
    sqlite3_stmt* stmt;
    bool exists = false;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        exists = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return exists;
}

bool FalconStore::updateWatchedFolderStatus(const std::string& path, int statusId) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "UPDATE watched_folders SET status_id = ? WHERE path = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, statusId);
    sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.updateQueries++;
    }
    
    return success;
}

// ============================================================================
// Bulk File Operations
// ============================================================================

std::vector<FileMetadata> FalconStore::getFilesInFolder(const std::string& folderPath) {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::vector<FileMetadata> files;
    std::string pattern = folderPath;
    if (!pattern.empty() && pattern.back() != '/') pattern += '/';
    pattern += "%";
    
    const char* sql = "SELECT path, hash, modified, size, synced, version FROM files WHERE path LIKE ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FileMetadata file;
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            file.path = path && strlen(path) > 0 ? path : "";
            const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            file.hash = hash && strlen(hash) > 0 ? hash : "";
            file.timestamp = sqlite3_column_int64(stmt, 2);
            file.size = sqlite3_column_int64(stmt, 3);
            file.synced = sqlite3_column_int(stmt, 4);
            file.version = sqlite3_column_int(stmt, 5);
            files.push_back(file);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return files;
}

int FalconStore::removeFilesInFolder(const std::string& folderPath) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::string pattern = folderPath;
    if (!pattern.empty() && pattern.back() != '/') {
        pattern += '/';
    }
    pattern += '%';
    
    // Count first
    int count = 0;
    const char* countSql = "SELECT COUNT(*) FROM files WHERE path LIKE ?";
    sqlite3_stmt* countStmt;
    if (sqlite3_prepare_v2(impl_->db, countSql, -1, &countStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(countStmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(countStmt) == SQLITE_ROW) {
            count = sqlite3_column_int(countStmt, 0);
        }
        sqlite3_finalize(countStmt);
    }
    
    // Delete
    const char* sql = "DELETE FROM files WHERE path LIKE ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Invalidate cache for folder
    if (impl_->cache) {
        impl_->cache->clear();  // Clear all cache for bulk delete
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries += count;
    }
    
    return count;
}

int FalconStore::getFileCount() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    int count = 0;
    const char* sql = "SELECT COUNT(*) FROM files";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return count;
}

long long FalconStore::getTotalFileSize() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    long long total = 0;
    const char* sql = "SELECT COALESCE(SUM(size), 0) FROM files";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return total;
}

bool FalconStore::markFileSynced(const std::string& path, bool synced) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "UPDATE files SET synced = ? WHERE path = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, synced ? 1 : 0);
    sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    if (impl_->cache && success) {
        impl_->cache->invalidate("file:" + path);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.updateQueries++;
    }
    
    return success;
}

std::vector<FileMetadata> FalconStore::getPendingFiles() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::vector<FileMetadata> files;
    const char* sql = "SELECT path, hash, modified, size, synced, version FROM files WHERE synced = 0";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FileMetadata file;
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            file.path = path && strlen(path) > 0 ? path : "";
            const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            file.hash = hash && strlen(hash) > 0 ? hash : "";
            file.timestamp = sqlite3_column_int64(stmt, 2);
            file.size = sqlite3_column_int64(stmt, 3);
            file.synced = sqlite3_column_int(stmt, 4);
            file.version = sqlite3_column_int(stmt, 5);
            files.push_back(file);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return files;
}

// ============================================================================
// Ignore Patterns
// ============================================================================

bool FalconStore::addIgnorePattern(const std::string& pattern) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "INSERT OR IGNORE INTO ignore_patterns (pattern) VALUES (?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.insertQueries++;
    }
    
    return success;
}

bool FalconStore::removeIgnorePattern(const std::string& pattern) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "DELETE FROM ignore_patterns WHERE pattern = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries++;
    }
    
    return success;
}

std::vector<std::string> FalconStore::getIgnorePatterns() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::vector<std::string> patterns;
    const char* sql = "SELECT pattern FROM ignore_patterns";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* pattern = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (pattern) patterns.push_back(pattern);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return patterns;
}

// ============================================================================
// Threat Operations
// ============================================================================

bool FalconStore::addThreat(const ThreatInfo& threat) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    // Check if threat already exists for this file (prevent duplicates)
    const char* checkSql = "SELECT id FROM detected_threats WHERE file_path = ?";
    sqlite3_stmt* checkStmt;
    bool exists = false;
    
    if (sqlite3_prepare_v2(impl_->db, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(checkStmt, 1, threat.filePath.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            exists = true;
        }
        sqlite3_finalize(checkStmt);
    }
    
    // Skip insertion if threat already exists for this file
    if (exists) {
        auto end = std::chrono::steady_clock::now();
        impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
        return true; // Already tracked
    }
    
    const char* sql = R"(
        INSERT INTO detected_threats (file_path, threat_type_id, threat_level_id, threat_score, description)
        VALUES (?, 
            (SELECT id FROM threat_types WHERE name = ?),
            (SELECT id FROM threat_levels WHERE name = ?),
            ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, threat.filePath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, threat.threatType.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, threat.threatLevel.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, threat.threatScore);
    sqlite3_bind_text(stmt, 5, threat.description.c_str(), -1, SQLITE_STATIC);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.insertQueries++;
    }
    
    return success;
}

std::vector<ThreatInfo> FalconStore::getThreats() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::vector<ThreatInfo> threats;
    const char* sql = R"(
        SELECT dt.id, dt.file_path, tt.name, tl.name, dt.threat_score, 
               dt.detected_at, dt.description, dt.marked_safe
        FROM detected_threats dt
        LEFT JOIN threat_types tt ON dt.threat_type_id = tt.id
        LEFT JOIN threat_levels tl ON dt.threat_level_id = tl.id
        ORDER BY dt.detected_at DESC
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ThreatInfo threat;
            threat.id = sqlite3_column_int(stmt, 0);
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            threat.filePath = path ? path : "";
            const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            threat.threatType = type ? type : "";
            const char* level = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            threat.threatLevel = level ? level : "";
            threat.threatScore = sqlite3_column_double(stmt, 4);
            const char* detected = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            threat.detectedAt = detected ? detected : "";
            const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            threat.description = desc ? desc : "";
            threat.markedSafe = sqlite3_column_int(stmt, 7) != 0;
            threats.push_back(threat);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return threats;
}

bool FalconStore::removeThreat(int threatId) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "DELETE FROM detected_threats WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, threatId);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries++;
    }
    
    return success;
}

int FalconStore::removeThreatsInFolder(const std::string& folderPath) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::string pattern = folderPath;
    if (!pattern.empty() && pattern.back() != '/') {
        pattern += '/';
    }
    pattern += '%';
    
    // Delete threats where file_path starts with folder path (direct match)
    const char* sql = "DELETE FROM detected_threats WHERE file_path LIKE ?";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    int deleted = sqlite3_changes(impl_->db);
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries++;
    }
    
    return deleted;
}

bool FalconStore::markThreatSafe(int threatId, bool safe) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "UPDATE detected_threats SET marked_safe = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, safe ? 1 : 0);
    sqlite3_bind_int(stmt, 2, threatId);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.updateQueries++;
    }
    
    return success;
}

// ============================================================================
// Sync Queue Operations
// ============================================================================

std::vector<SyncQueueItem> FalconStore::getSyncQueue() {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::vector<SyncQueueItem> items;
    const char* sql = R"(
        SELECT sq.id, f.path, ot.name, st.name, sq.priority, sq.created_at
        FROM sync_queue sq
        LEFT JOIN files f ON sq.file_id = f.id
        LEFT JOIN op_types ot ON sq.op_type_id = ot.id
        LEFT JOIN status_types st ON sq.status_id = st.id
        ORDER BY sq.priority DESC, sq.created_at ASC
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SyncQueueItem item;
            item.id = sqlite3_column_int(stmt, 0);
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            item.filePath = path ? path : "";
            const char* op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            item.opType = op ? op : "";
            const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            item.status = status ? status : "";
            item.priority = sqlite3_column_int(stmt, 4);
            item.createdAt = sqlite3_column_int64(stmt, 5);
            items.push_back(item);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return items;
}

bool FalconStore::updateSyncQueueStatus(int itemId, const std::string& status) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "UPDATE sync_queue SET status_id = (SELECT id FROM status_types WHERE name = ?) WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, itemId);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.updateQueries++;
    }
    
    return success;
}

int FalconStore::clearCompletedSyncOperations() {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    // Count first
    int count = 0;
    const char* countSql = "SELECT COUNT(*) FROM sync_queue WHERE status_id = (SELECT id FROM status_types WHERE name = 'completed')";
    sqlite3_stmt* countStmt;
    if (sqlite3_prepare_v2(impl_->db, countSql, -1, &countStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(countStmt) == SQLITE_ROW) {
            count = sqlite3_column_int(countStmt, 0);
        }
        sqlite3_finalize(countStmt);
    }
    
    sqlite3_exec(impl_->db, "DELETE FROM sync_queue WHERE status_id = (SELECT id FROM status_types WHERE name = 'completed')", nullptr, nullptr, nullptr);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries += count;
    }
    
    return count;
}

// ============================================================================
// Activity Log
// ============================================================================

std::vector<ActivityLogEntry> FalconStore::getRecentActivity(int limit) {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::vector<ActivityLogEntry> entries;
    const char* sql = R"(
        SELECT al.id, f.path, ot.name, al.timestamp, al.peer_id, al.details
        FROM activity_log al
        LEFT JOIN files f ON al.file_id = f.id
        LEFT JOIN op_types ot ON al.op_type_id = ot.id
        ORDER BY al.timestamp DESC
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ActivityLogEntry entry;
            entry.id = sqlite3_column_int(stmt, 0);
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            entry.filePath = path ? path : "";
            const char* op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            entry.opType = op ? op : "";
            entry.timestamp = sqlite3_column_int64(stmt, 3);
            const char* peer = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            entry.peerId = peer ? peer : "";
            const char* details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            entry.details = details ? details : "";
            entries.push_back(entry);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return entries;
}

// ============================================================================
// Peer Extended Operations
// ============================================================================

bool FalconStore::removeAllPeers() {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    // Count first
    int count = 0;
    const char* countSql = "SELECT COUNT(*) FROM peers";
    sqlite3_stmt* countStmt;
    if (sqlite3_prepare_v2(impl_->db, countSql, -1, &countStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(countStmt) == SQLITE_ROW) {
            count = sqlite3_column_int(countStmt, 0);
        }
        sqlite3_finalize(countStmt);
    }
    
    sqlite3_exec(impl_->db, "DELETE FROM peers", nullptr, nullptr, nullptr);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries += count;
    }
    
    return true;
}

bool FalconStore::updatePeerStatus(const std::string& peerId, const std::string& status) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "UPDATE peers SET status_id = (SELECT id FROM status_types WHERE name = ?) WHERE peer_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, peerId.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.updateQueries++;
    }
    
    return success;
}

bool FalconStore::blockPeer(const std::string& peerId) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    // Ensure blocked_peers table exists
    sqlite3_exec(impl_->db, "CREATE TABLE IF NOT EXISTS blocked_peers (peer_id TEXT PRIMARY KEY, blocked_at DATETIME DEFAULT CURRENT_TIMESTAMP)", nullptr, nullptr, nullptr);
    
    // Add to blocked list
    const char* sql = "INSERT OR REPLACE INTO blocked_peers (peer_id) VALUES (?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    // Also remove from peers table
    if (success) {
        const char* delSql = "DELETE FROM peers WHERE peer_id = ?";
        sqlite3_stmt* delStmt;
        if (sqlite3_prepare_v2(impl_->db, delSql, -1, &delStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(delStmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(delStmt);
            sqlite3_finalize(delStmt);
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.insertQueries++;
        impl_->stats.deleteQueries++;
    }
    
    return success;
}

bool FalconStore::unblockPeer(const std::string& peerId) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "DELETE FROM blocked_peers WHERE peer_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries++;
    }
    
    return success;
}

bool FalconStore::isPeerBlocked(const std::string& peerId) {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "SELECT 1 FROM blocked_peers WHERE peer_id = ? LIMIT 1";
    sqlite3_stmt* stmt;
    bool blocked = false;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
        blocked = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return blocked;
}

// ============================================================================
// Config Storage
// ============================================================================

bool FalconStore::setConfig(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    // Ensure config table exists
    sqlite3_exec(impl_->db, "CREATE TABLE IF NOT EXISTS config (key TEXT PRIMARY KEY, value TEXT)", nullptr, nullptr, nullptr);
    
    const char* sql = "INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.insertQueries++;
    }
    
    return success;
}

std::optional<std::string> FalconStore::getConfig(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::optional<std::string> result;
    const char* sql = "SELECT value FROM config WHERE key = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (value) result = value;
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return result;
}

bool FalconStore::removeConfig(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    const char* sql = "DELETE FROM config WHERE key = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.deleteQueries++;
    }
    
    return success;
}

// ============================================================================
// Transfer History
// ============================================================================

bool FalconStore::logTransfer(const std::string& filePath, const std::string& peerId, 
                              const std::string& direction, long long bytes, bool success) {
    std::unique_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    // Ensure transfer_history table exists
    sqlite3_exec(impl_->db, R"(
        CREATE TABLE IF NOT EXISTS transfer_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT,
            peer_id TEXT,
            direction TEXT,
            bytes INTEGER,
            success INTEGER,
            timestamp INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )", nullptr, nullptr, nullptr);
    
    const char* sql = "INSERT INTO transfer_history (file_path, peer_id, direction, bytes, success) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, peerId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, direction.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, bytes);
    sqlite3_bind_int(stmt, 5, success ? 1 : 0);
    
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.insertQueries++;
    }
    
    return ok;
}

std::vector<std::pair<std::string, long long>> FalconStore::getTransferHistory(int limit) {
    std::shared_lock<std::shared_mutex> lock(impl_->dbMutex);
    auto start = std::chrono::steady_clock::now();
    
    std::vector<std::pair<std::string, long long>> history;
    const char* sql = "SELECT file_path, bytes FROM transfer_history ORDER BY timestamp DESC LIMIT ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            long long bytes = sqlite3_column_int64(stmt, 1);
            history.emplace_back(path ? path : "", bytes);
        }
        sqlite3_finalize(stmt);
    }
    
    auto end = std::chrono::steady_clock::now();
    impl_->updateQueryStats(std::chrono::duration<double, std::milli>(end - start).count());
    
    {
        std::lock_guard<std::mutex> slock(impl_->statsMutex);
        impl_->stats.selectQueries++;
    }
    
    return history;
}

// ============================================================================
// Plugin Factory
// ============================================================================

extern "C" {
    IPlugin* create_plugin() {
        return new FalconStore();
    }
    
    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace SentinelFS
