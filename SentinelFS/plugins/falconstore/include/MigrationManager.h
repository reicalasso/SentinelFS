/**
 * @file MigrationManager.h
 * @brief Schema migration system for FalconStore
 */

#pragma once

#include "FalconStore.h"
#include <sqlite3.h>
#include <vector>
#include <map>

namespace SentinelFS {
namespace Falcon {

/**
 * @brief Migration manager implementation
 */
class MigrationManager : public IMigrationManager {
public:
    explicit MigrationManager(sqlite3* db);
    ~MigrationManager() override = default;
    
    void registerMigration(const Migration& migration) override;
    int getCurrentVersion() const override;
    int getLatestVersion() const override;
    bool migrateUp(int targetVersion = -1) override;
    bool migrateDown(int targetVersion) override;
    std::vector<Migration> getPendingMigrations() const override;
    
    /**
     * @brief Register default SentinelFS schema migrations
     */
    void registerDefaultMigrations();

private:
    sqlite3* db_;
    std::map<int, Migration> migrations_;
    
    bool ensureMigrationTable();
    bool setVersion(int version);
    bool executeMigration(const Migration& migration, bool up);
};

// ============================================================================
// Default Migrations
// ============================================================================

inline void MigrationManager::registerDefaultMigrations() {
    // Version 1: Initial schema
    registerMigration({
        1, "Initial schema",
        R"(
            -- Lookup tables
            CREATE TABLE IF NOT EXISTS op_types (
                id INTEGER PRIMARY KEY,
                name TEXT UNIQUE NOT NULL
            );
            
            CREATE TABLE IF NOT EXISTS status_types (
                id INTEGER PRIMARY KEY,
                name TEXT UNIQUE NOT NULL
            );
            
            CREATE TABLE IF NOT EXISTS threat_types (
                id INTEGER PRIMARY KEY,
                name TEXT UNIQUE NOT NULL
            );
            
            CREATE TABLE IF NOT EXISTS threat_levels (
                id INTEGER PRIMARY KEY,
                name TEXT UNIQUE NOT NULL
            );
            
            -- Core tables
            CREATE TABLE IF NOT EXISTS files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                path TEXT UNIQUE NOT NULL,
                hash TEXT,
                size INTEGER NOT NULL,
                modified INTEGER NOT NULL,
                synced INTEGER DEFAULT 0,
                version INTEGER DEFAULT 1,
                created_at INTEGER DEFAULT (strftime('%s', 'now'))
            );
            
            CREATE TABLE IF NOT EXISTS peers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                peer_id TEXT UNIQUE NOT NULL,
                name TEXT,
                address TEXT NOT NULL,
                port INTEGER NOT NULL,
                public_key TEXT,
                status_id INTEGER DEFAULT 1,
                last_seen INTEGER,
                latency INTEGER,
                FOREIGN KEY(status_id) REFERENCES status_types(id)
            );
            
            CREATE TABLE IF NOT EXISTS conflicts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_id INTEGER NOT NULL,
                local_hash TEXT,
                remote_hash TEXT,
                local_size INTEGER,
                remote_size INTEGER,
                local_timestamp INTEGER,
                remote_timestamp INTEGER,
                remote_peer_id TEXT,
                detected_at INTEGER DEFAULT (strftime('%s', 'now')),
                resolved INTEGER DEFAULT 0,
                resolution TEXT,
                strategy TEXT DEFAULT 'manual',
                FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE
            );
            
            CREATE TABLE IF NOT EXISTS watched_folders (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                path TEXT UNIQUE NOT NULL,
                added_at INTEGER DEFAULT (strftime('%s', 'now')),
                status_id INTEGER DEFAULT 1,
                FOREIGN KEY(status_id) REFERENCES status_types(id)
            );
            
            -- Indexes
            CREATE INDEX IF NOT EXISTS idx_files_path ON files(path);
            CREATE INDEX IF NOT EXISTS idx_files_hash ON files(hash);
            CREATE INDEX IF NOT EXISTS idx_files_synced ON files(synced);
            CREATE INDEX IF NOT EXISTS idx_peers_status ON peers(status_id);
            CREATE INDEX IF NOT EXISTS idx_conflicts_file ON conflicts(file_id);
            CREATE INDEX IF NOT EXISTS idx_conflicts_resolved ON conflicts(resolved);
            
            -- Populate lookup tables
            INSERT OR IGNORE INTO op_types (id, name) VALUES 
                (1, 'create'), (2, 'update'), (3, 'delete'), 
                (4, 'read'), (5, 'write'), (6, 'rename'), (7, 'move');
            
            INSERT OR IGNORE INTO status_types (id, name) VALUES 
                (1, 'active'), (2, 'pending'), (3, 'syncing'), 
                (4, 'completed'), (5, 'failed'), (6, 'offline'), (7, 'paused');
        )",
        R"(
            DROP TABLE IF EXISTS watched_folders;
            DROP TABLE IF EXISTS conflicts;
            DROP TABLE IF EXISTS peers;
            DROP TABLE IF EXISTS files;
            DROP TABLE IF EXISTS threat_levels;
            DROP TABLE IF EXISTS threat_types;
            DROP TABLE IF EXISTS status_types;
            DROP TABLE IF EXISTS op_types;
        )",
        nullptr, nullptr
    });
    
    // Version 2: Threat detection tables
    registerMigration({
        2, "Threat detection",
        R"(
            CREATE TABLE IF NOT EXISTS detected_threats (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_id INTEGER,
                file_path TEXT NOT NULL,
                threat_type_id INTEGER NOT NULL,
                threat_level_id INTEGER NOT NULL,
                threat_score REAL NOT NULL,
                detected_at TEXT NOT NULL,
                entropy REAL,
                file_size INTEGER NOT NULL,
                hash TEXT,
                quarantine_path TEXT,
                ml_model_used TEXT,
                additional_info TEXT,
                marked_safe INTEGER DEFAULT 0,
                FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE,
                FOREIGN KEY(threat_type_id) REFERENCES threat_types(id),
                FOREIGN KEY(threat_level_id) REFERENCES threat_levels(id)
            );
            
            CREATE INDEX IF NOT EXISTS idx_detected_threats_file ON detected_threats(file_id);
            CREATE INDEX IF NOT EXISTS idx_detected_threats_level ON detected_threats(threat_level_id);
            CREATE INDEX IF NOT EXISTS idx_detected_threats_detected ON detected_threats(detected_at);
            CREATE INDEX IF NOT EXISTS idx_detected_threats_path ON detected_threats(file_path);
            
            -- Zer0 compatible threat types
            INSERT OR IGNORE INTO threat_types (id, name) VALUES 
                (0, 'UNKNOWN'),
                (1, 'RANSOMWARE_PATTERN'),
                (2, 'HIGH_ENTROPY_TEXT'),
                (3, 'HIDDEN_EXECUTABLE'),
                (4, 'EXTENSION_MISMATCH'),
                (5, 'DOUBLE_EXTENSION'),
                (6, 'MASS_MODIFICATION'),
                (7, 'SCRIPT_IN_DATA'),
                (8, 'ANOMALOUS_BEHAVIOR'),
                (9, 'KNOWN_MALWARE_HASH'),
                (10, 'SUSPICIOUS_RENAME');
            
            -- Zer0 compatible threat levels
            INSERT OR IGNORE INTO threat_levels (id, name) VALUES 
                (0, 'NONE'), (1, 'INFO'), (2, 'LOW'), 
                (3, 'MEDIUM'), (4, 'HIGH'), (5, 'CRITICAL');
        )",
        R"(
            DROP INDEX IF EXISTS idx_detected_threats_path;
            DROP INDEX IF EXISTS idx_detected_threats_detected;
            DROP INDEX IF EXISTS idx_detected_threats_level;
            DROP INDEX IF EXISTS idx_detected_threats_file;
            DROP TABLE IF EXISTS detected_threats;
        )",
        nullptr, nullptr
    });
    
    // Version 3: File versioning
    registerMigration({
        3, "File versioning",
        R"(
            CREATE TABLE IF NOT EXISTS file_versions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_id INTEGER NOT NULL,
                version INTEGER NOT NULL,
                hash TEXT NOT NULL,
                size INTEGER NOT NULL,
                created_at INTEGER DEFAULT (strftime('%s', 'now')),
                created_by TEXT,
                delta_path TEXT,
                FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE
            );
            
            CREATE INDEX IF NOT EXISTS idx_file_versions_file ON file_versions(file_id);
            CREATE INDEX IF NOT EXISTS idx_file_versions_version ON file_versions(version);
        )",
        R"(
            DROP INDEX IF EXISTS idx_file_versions_version;
            DROP INDEX IF EXISTS idx_file_versions_file;
            DROP TABLE IF EXISTS file_versions;
        )",
        nullptr, nullptr
    });
    
    // Version 4: Sync queue
    registerMigration({
        4, "Sync queue",
        R"(
            CREATE TABLE IF NOT EXISTS sync_queue (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_id INTEGER NOT NULL,
                peer_id TEXT NOT NULL,
                op_type_id INTEGER NOT NULL,
                status_id INTEGER DEFAULT 2,
                priority INTEGER DEFAULT 5,
                created_at INTEGER DEFAULT (strftime('%s', 'now')),
                started_at INTEGER,
                completed_at INTEGER,
                retry_count INTEGER DEFAULT 0,
                error_message TEXT,
                FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE,
                FOREIGN KEY(op_type_id) REFERENCES op_types(id),
                FOREIGN KEY(status_id) REFERENCES status_types(id)
            );
            
            CREATE INDEX IF NOT EXISTS idx_sync_queue_status ON sync_queue(status_id);
            CREATE INDEX IF NOT EXISTS idx_sync_queue_file ON sync_queue(file_id);
            CREATE INDEX IF NOT EXISTS idx_sync_queue_priority ON sync_queue(priority);
        )",
        R"(
            DROP INDEX IF EXISTS idx_sync_queue_priority;
            DROP INDEX IF EXISTS idx_sync_queue_file;
            DROP INDEX IF EXISTS idx_sync_queue_status;
            DROP TABLE IF EXISTS sync_queue;
        )",
        nullptr, nullptr
    });
    
    // Version 5: Activity log
    registerMigration({
        5, "Activity log",
        R"(
            CREATE TABLE IF NOT EXISTS activity_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_id INTEGER,
                op_type_id INTEGER NOT NULL,
                timestamp INTEGER DEFAULT (strftime('%s', 'now')),
                details TEXT,
                peer_id TEXT,
                FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE SET NULL,
                FOREIGN KEY(op_type_id) REFERENCES op_types(id)
            );
            
            CREATE INDEX IF NOT EXISTS idx_activity_log_file ON activity_log(file_id);
            CREATE INDEX IF NOT EXISTS idx_activity_log_timestamp ON activity_log(timestamp);
            CREATE INDEX IF NOT EXISTS idx_activity_log_op ON activity_log(op_type_id);
        )",
        R"(
            DROP INDEX IF EXISTS idx_activity_log_op;
            DROP INDEX IF EXISTS idx_activity_log_timestamp;
            DROP INDEX IF EXISTS idx_activity_log_file;
            DROP TABLE IF EXISTS activity_log;
        )",
        nullptr, nullptr
    });
    
    // Version 6: Ignore patterns
    registerMigration({
        6, "Ignore patterns",
        R"(
            CREATE TABLE IF NOT EXISTS ignore_patterns (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                pattern TEXT UNIQUE NOT NULL,
                created_at INTEGER DEFAULT (strftime('%s', 'now'))
            );
        )",
        R"(
            DROP TABLE IF EXISTS ignore_patterns;
        )",
        nullptr, nullptr
    });
    
    // Version 7: Legacy schema compatibility - add 'modified' column if missing
    registerMigration({
        7, "Legacy schema compatibility",
        "",  // SQL handled by callback
        "",
        // Up callback - add modified column and copy from timestamp
        [](void* dbPtr) -> bool {
            sqlite3* db = static_cast<sqlite3*>(dbPtr);
            // Check if 'modified' column exists
            sqlite3_stmt* stmt;
            bool hasModified = false;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(files)", -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    if (colName && std::string(colName) == "modified") {
                        hasModified = true;
                        break;
                    }
                }
                sqlite3_finalize(stmt);
            }
            
            if (!hasModified) {
                // Add modified column
                char* errMsg = nullptr;
                if (sqlite3_exec(db, "ALTER TABLE files ADD COLUMN modified INTEGER", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                    if (errMsg) sqlite3_free(errMsg);
                    return false;
                }
                
                // Copy timestamp to modified
                if (sqlite3_exec(db, "UPDATE files SET modified = timestamp WHERE modified IS NULL", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                    if (errMsg) sqlite3_free(errMsg);
                    return false;
                }
            }
            
            // Check if 'version' column exists
            bool hasVersion = false;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(files)", -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    if (colName && std::string(colName) == "version") {
                        hasVersion = true;
                        break;
                    }
                }
                sqlite3_finalize(stmt);
            }
            
            if (!hasVersion) {
                char* errMsg = nullptr;
                if (sqlite3_exec(db, "ALTER TABLE files ADD COLUMN version INTEGER DEFAULT 1", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                    if (errMsg) sqlite3_free(errMsg);
                    return false;
                }
            }
            
            // Check if 'created_at' column exists
            bool hasCreatedAt = false;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(files)", -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    if (colName && std::string(colName) == "created_at") {
                        hasCreatedAt = true;
                        break;
                    }
                }
                sqlite3_finalize(stmt);
            }
            
            if (!hasCreatedAt) {
                char* errMsg = nullptr;
                if (sqlite3_exec(db, "ALTER TABLE files ADD COLUMN created_at INTEGER DEFAULT (strftime('%s', 'now'))", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                    if (errMsg) sqlite3_free(errMsg);
                    // Not critical, continue
                }
            }
            
            return true;
        },
        nullptr  // No down callback needed
    });
    
    // Version 8: Standardize peers table schema
    registerMigration({
        8, "Standardize peers table schema",
        "",  // SQL handled by callback
        "",
        // Up callback - recreate peers table with proper constraints
        [](void* dbPtr) -> bool {
            sqlite3* db = static_cast<sqlite3*>(dbPtr);
            char* errMsg = nullptr;
            
            // Create new peers table with proper schema
            const char* createNewTable = R"(
                CREATE TABLE IF NOT EXISTS peers_new (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    peer_id TEXT UNIQUE NOT NULL,
                    name TEXT NOT NULL,
                    address TEXT NOT NULL DEFAULT '',
                    port INTEGER NOT NULL DEFAULT 0,
                    public_key TEXT,
                    status_id INTEGER NOT NULL DEFAULT 6,
                    last_seen INTEGER NOT NULL DEFAULT 0,
                    latency INTEGER NOT NULL DEFAULT 0,
                    FOREIGN KEY(status_id) REFERENCES status_types(id)
                )
            )";
            
            if (sqlite3_exec(db, createNewTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            // Copy data from old table with proper defaults
            const char* copyData = R"(
                INSERT INTO peers_new (id, peer_id, name, address, port, public_key, status_id, last_seen, latency)
                SELECT 
                    id,
                    peer_id,
                    COALESCE(name, peer_id),
                    COALESCE(address, ''),
                    COALESCE(port, 0),
                    public_key,
                    COALESCE(status_id, 6),
                    COALESCE(last_seen, 0),
                    COALESCE(latency, 0)
                FROM peers
            )";
            
            if (sqlite3_exec(db, copyData, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                // Table might not exist yet, that's ok
            }
            
            // Drop old table and rename new one
            if (sqlite3_exec(db, "DROP TABLE IF EXISTS peers", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            if (sqlite3_exec(db, "ALTER TABLE peers_new RENAME TO peers", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            // Recreate index
            if (sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_peers_status ON peers(status_id)", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                // Not critical
            }
            
            return true;
        },
        nullptr  // No down callback needed
    });
    
    // Version 9: Standardize all remaining tables
    registerMigration({
        9, "Standardize files, conflicts, and watched_folders tables",
        "",  // SQL handled by callback
        "",
        // Up callback - standardize remaining tables
        [](void* dbPtr) -> bool {
            sqlite3* db = static_cast<sqlite3*>(dbPtr);
            char* errMsg = nullptr;
            
            // ===== Standardize files table =====
            const char* createFilesNew = R"(
                CREATE TABLE IF NOT EXISTS files_new (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    path TEXT UNIQUE NOT NULL,
                    hash TEXT NOT NULL DEFAULT '',
                    size INTEGER NOT NULL DEFAULT 0,
                    modified INTEGER NOT NULL DEFAULT 0,
                    synced INTEGER NOT NULL DEFAULT 0,
                    version INTEGER NOT NULL DEFAULT 1,
                    created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
                )
            )";
            
            if (sqlite3_exec(db, createFilesNew, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            // Copy files data with proper defaults
            const char* copyFiles = R"(
                INSERT INTO files_new (id, path, hash, size, modified, synced, version, created_at)
                SELECT 
                    id,
                    path,
                    COALESCE(hash, ''),
                    COALESCE(size, 0),
                    COALESCE(modified, 0),
                    COALESCE(synced, 0),
                    COALESCE(version, 1),
                    COALESCE(created_at, strftime('%s', 'now'))
                FROM files
            )";
            
            if (sqlite3_exec(db, copyFiles, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                // Table might not exist, continue
            }
            
            if (sqlite3_exec(db, "DROP TABLE IF EXISTS files", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            if (sqlite3_exec(db, "ALTER TABLE files_new RENAME TO files", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            // Recreate indexes
            sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_files_path ON files(path)", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_files_hash ON files(hash)", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_files_synced ON files(synced)", nullptr, nullptr, nullptr);
            
            // ===== Standardize conflicts table =====
            const char* createConflictsNew = R"(
                CREATE TABLE IF NOT EXISTS conflicts_new (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    file_id INTEGER NOT NULL,
                    local_hash TEXT NOT NULL DEFAULT '',
                    remote_hash TEXT NOT NULL DEFAULT '',
                    local_size INTEGER NOT NULL DEFAULT 0,
                    remote_size INTEGER NOT NULL DEFAULT 0,
                    local_timestamp INTEGER NOT NULL DEFAULT 0,
                    remote_timestamp INTEGER NOT NULL DEFAULT 0,
                    remote_peer_id TEXT NOT NULL DEFAULT '',
                    detected_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                    resolved INTEGER NOT NULL DEFAULT 0,
                    resolution TEXT NOT NULL DEFAULT '',
                    strategy TEXT NOT NULL DEFAULT 'manual',
                    FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE
                )
            )";
            
            if (sqlite3_exec(db, createConflictsNew, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            // Copy conflicts data with proper defaults
            const char* copyConflicts = R"(
                INSERT INTO conflicts_new (id, file_id, local_hash, remote_hash, local_size, remote_size, 
                                          local_timestamp, remote_timestamp, remote_peer_id, detected_at, 
                                          resolved, resolution, strategy)
                SELECT 
                    id,
                    file_id,
                    COALESCE(local_hash, ''),
                    COALESCE(remote_hash, ''),
                    COALESCE(local_size, 0),
                    COALESCE(remote_size, 0),
                    COALESCE(local_timestamp, 0),
                    COALESCE(remote_timestamp, 0),
                    COALESCE(remote_peer_id, ''),
                    COALESCE(detected_at, strftime('%s', 'now')),
                    COALESCE(resolved, 0),
                    COALESCE(resolution, ''),
                    COALESCE(strategy, 'manual')
                FROM conflicts
            )";
            
            if (sqlite3_exec(db, copyConflicts, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                // Table might not exist, continue
            }
            
            if (sqlite3_exec(db, "DROP TABLE IF EXISTS conflicts", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            if (sqlite3_exec(db, "ALTER TABLE conflicts_new RENAME TO conflicts", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            // Recreate indexes
            sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_conflicts_file ON conflicts(file_id)", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_conflicts_resolved ON conflicts(resolved)", nullptr, nullptr, nullptr);
            
            // ===== Standardize watched_folders table =====
            const char* createWatchedNew = R"(
                CREATE TABLE IF NOT EXISTS watched_folders_new (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    path TEXT UNIQUE NOT NULL,
                    added_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                    status_id INTEGER NOT NULL DEFAULT 1,
                    FOREIGN KEY(status_id) REFERENCES status_types(id)
                )
            )";
            
            if (sqlite3_exec(db, createWatchedNew, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            // Copy watched_folders data with proper defaults
            const char* copyWatched = R"(
                INSERT INTO watched_folders_new (id, path, added_at, status_id)
                SELECT 
                    id,
                    path,
                    COALESCE(added_at, strftime('%s', 'now')),
                    COALESCE(status_id, 1)
                FROM watched_folders
            )";
            
            if (sqlite3_exec(db, copyWatched, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                // Table might not exist, continue
            }
            
            if (sqlite3_exec(db, "DROP TABLE IF EXISTS watched_folders", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            if (sqlite3_exec(db, "ALTER TABLE watched_folders_new RENAME TO watched_folders", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                if (errMsg) sqlite3_free(errMsg);
                return false;
            }
            
            return true;
        },
        nullptr  // No down callback needed
    });
}

} // namespace Falcon
} // namespace SentinelFS
