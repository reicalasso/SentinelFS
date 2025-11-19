#include "IStorageAPI.h"
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <chrono>

namespace SentinelFS {

    class StoragePlugin : public IStorageAPI {
    public:
        bool initialize(EventBus* eventBus) override {
            std::cout << "StoragePlugin initialized" << std::endl;
            if (sqlite3_open("sentinel.db", &db_) != SQLITE_OK) {
                std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
                return false;
            }
            return createTables();
        }

        void shutdown() override {
            std::cout << "StoragePlugin shutdown" << std::endl;
            if (db_) {
                sqlite3_close(db_);
            }
        }

        std::string getName() const override {
            return "StoragePlugin";
        }

        std::string getVersion() const override {
            return "1.0.0";
        }

        public:
        // CRUD Operations for Files
        bool addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) override {
            const char* sql = "INSERT OR REPLACE INTO files (path, hash, timestamp, size) VALUES (?, ?, ?, ?);";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
                return false;
            }

            sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 3, timestamp);
            sqlite3_bind_int64(stmt, 4, size);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }

            sqlite3_finalize(stmt);
            return true;
        }

        std::optional<FileMetadata> getFile(const std::string& path) override {
            const char* sql = "SELECT path, hash, timestamp, size FROM files WHERE path = ?;";
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
                return std::nullopt;
            }

            sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);

            std::optional<FileMetadata> result = std::nullopt;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                FileMetadata meta;
                meta.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                meta.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                meta.timestamp = sqlite3_column_int64(stmt, 2);
                meta.size = sqlite3_column_int64(stmt, 3);
                result = meta;
            }

            sqlite3_finalize(stmt);
            return result;
        }

        bool removeFile(const std::string& path) override {
            const char* sql = "DELETE FROM files WHERE path = ?;";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
                return false;
            }

            sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }

            sqlite3_finalize(stmt);
            return true;
        }

        // --- Peer Operations ---

        bool addPeer(const PeerInfo& peer) override {
            const char* sql = "INSERT OR REPLACE INTO peers (id, address, port, last_seen, status, latency) VALUES (?, ?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
                return false;
            }

            sqlite3_bind_text(stmt, 1, peer.id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, peer.ip.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, peer.port);
            sqlite3_bind_int64(stmt, 4, peer.lastSeen);
            sqlite3_bind_text(stmt, 5, peer.status.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 6, peer.latency);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }

            sqlite3_finalize(stmt);
            return true;
        }

        std::optional<PeerInfo> getPeer(const std::string& peerId) override {
            const char* sql = "SELECT id, address, port, last_seen, status, latency FROM peers WHERE id = ?;";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
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

        std::vector<PeerInfo> getAllPeers() override {
            std::vector<PeerInfo> peers;
            const char* sql = "SELECT id, address, port, last_seen, status, latency FROM peers;";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
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

        bool updatePeerLatency(const std::string& peerId, int latency) override {
            const char* sql = "UPDATE peers SET latency = ? WHERE id = ?;";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
                return false;
            }

            sqlite3_bind_int(stmt, 1, latency);
            sqlite3_bind_text(stmt, 2, peerId.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }

            sqlite3_finalize(stmt);
            return true;
        }

        std::vector<PeerInfo> getPeersByLatency() override {
            std::vector<PeerInfo> peers;
            const char* sql = "SELECT id, address, port, last_seen, status, latency FROM peers ORDER BY CASE WHEN latency = -1 THEN 999999 ELSE latency END ASC;";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
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
        
        // --- Conflict Operations ---
        
        bool addConflict(const ConflictInfo& conflict) override {
            const char* sql = "INSERT INTO conflicts (path, local_hash, remote_hash, remote_peer_id, "
                             "local_timestamp, remote_timestamp, local_size, remote_size, strategy, "
                             "resolved, detected_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
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
                std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }

            sqlite3_finalize(stmt);
            return true;
        }
        
        std::vector<ConflictInfo> getUnresolvedConflicts() override {
            const char* sql = "SELECT id, path, local_hash, remote_hash, remote_peer_id, "
                             "local_timestamp, remote_timestamp, local_size, remote_size, "
                             "strategy, resolved, detected_at, resolved_at "
                             "FROM conflicts WHERE resolved = 0 ORDER BY detected_at DESC;";
            return queryConflicts(sql);
        }
        
        std::vector<ConflictInfo> getConflictsForFile(const std::string& path) override {
            const char* sql = "SELECT id, path, local_hash, remote_hash, remote_peer_id, "
                             "local_timestamp, remote_timestamp, local_size, remote_size, "
                             "strategy, resolved, detected_at, resolved_at "
                             "FROM conflicts WHERE path = ? ORDER BY detected_at DESC;";
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
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
        
        bool markConflictResolved(int conflictId) override {
            const char* sql = "UPDATE conflicts SET resolved = 1, resolved_at = ? WHERE id = ?;";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
                return false;
            }

            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            
            sqlite3_bind_int64(stmt, 1, timestamp);
            sqlite3_bind_int(stmt, 2, conflictId);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }

            sqlite3_finalize(stmt);
            return true;
        }
        
        std::pair<int, int> getConflictStats() override {
            const char* sql = "SELECT COUNT(*) as total, "
                             "SUM(CASE WHEN resolved = 0 THEN 1 ELSE 0 END) as unresolved "
                             "FROM conflicts;";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
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

    private:
        sqlite3* db_ = nullptr;
        
        std::vector<ConflictInfo> queryConflicts(const char* sql) {
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
                return {};
            }

            std::vector<ConflictInfo> conflicts;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                conflicts.push_back(parseConflictRow(stmt));
            }

            sqlite3_finalize(stmt);
            return conflicts;
        }
        
        ConflictInfo parseConflictRow(sqlite3_stmt* stmt) {
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

        bool createTables() {
            const char* sql = 
                "CREATE TABLE IF NOT EXISTS files ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "path TEXT UNIQUE NOT NULL,"
                "hash TEXT,"
                "timestamp INTEGER,"
                "size INTEGER,"
                "vector_clock TEXT);"  // Added vector clock tracking
                
                "CREATE TABLE IF NOT EXISTS peers ("
                "id TEXT PRIMARY KEY,"
                "address TEXT,"
                "port INTEGER,"
                "last_seen INTEGER,"
                "status TEXT,"
                "latency INTEGER DEFAULT -1);"
                
                "CREATE TABLE IF NOT EXISTS config ("
                "key TEXT PRIMARY KEY,"
                "value TEXT);"
                
                "CREATE TABLE IF NOT EXISTS conflicts ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "path TEXT NOT NULL,"
                "local_hash TEXT,"
                "remote_hash TEXT,"
                "remote_peer_id TEXT,"
                "local_timestamp INTEGER,"
                "remote_timestamp INTEGER,"
                "local_size INTEGER,"
                "remote_size INTEGER,"
                "strategy INTEGER,"  // ResolutionStrategy enum
                "resolved INTEGER DEFAULT 0,"  // 0=pending, 1=resolved
                "detected_at INTEGER,"
                "resolved_at INTEGER);";

            char* errMsg = nullptr;
            if (sqlite3_exec(db_, sql, 0, 0, &errMsg) != SQLITE_OK) {
                std::cerr << "SQL error: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                return false;
            }
            return true;
        }
    };

    extern "C" {
        IPlugin* create_plugin() {
            return new StoragePlugin();
        }

        void destroy_plugin(IPlugin* plugin) {
            delete plugin;
        }
    }

}