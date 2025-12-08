#include "SQLiteHandler.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>

namespace SentinelFS {

SQLiteHandler::~SQLiteHandler() {
    shutdown();
}

bool SQLiteHandler::initialize(const std::string& dbPath) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();

    std::string resolvedPath = dbPath;
    if (resolvedPath.empty()) {
        if (const char* envPath = std::getenv("SENTINEL_DB_PATH")) {
            resolvedPath = envPath;
        } else {
            // Use XDG data directory for database (writable location)
            std::filesystem::path dataDir;
            if (const char* xdgData = std::getenv("XDG_DATA_HOME")) {
                dataDir = std::filesystem::path(xdgData) / "sentinelfs";
            } else if (const char* home = std::getenv("HOME")) {
                dataDir = std::filesystem::path(home) / ".local" / "share" / "sentinelfs";
            } else {
                dataDir = "/tmp/sentinelfs";
            }
            resolvedPath = (dataDir / "sentinel.db").string();
        }
    }

    // Ensure directory exists using std::filesystem (safe, no shell injection risk)
    std::string dirPath;
    size_t lastSlash = resolvedPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        dirPath = resolvedPath.substr(0, lastSlash);
    }

    if (!dirPath.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dirPath, ec);
        if (ec) {
            logger.log(LogLevel::ERROR, "Failed to create database directory: " + dirPath + " (" + ec.message() + ")", "SQLiteHandler");
        }
    }

    logger.log(LogLevel::INFO, "Initializing SQLite database: " + resolvedPath, "SQLiteHandler");

    if (sqlite3_open(resolvedPath.c_str(), &db_) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Cannot open database: " + std::string(sqlite3_errmsg(db_)), "SQLiteHandler");
        metrics.incrementSyncErrors();
        return false;
    }

    logger.log(LogLevel::INFO, "Database opened successfully", "SQLiteHandler");

    // Enable WAL mode for better concurrency
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::WARN, "Failed to enable WAL mode: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
    } else {
        logger.log(LogLevel::INFO, "WAL mode enabled", "SQLiteHandler");
    }
    
    // Set busy timeout to 5000ms to handle contention
    sqlite3_busy_timeout(db_, 5000);

    // Simple schema versioning using PRAGMA user_version
    int userVersion = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA user_version;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            userVersion = sqlite3_column_int(stmt, 0);
        }
    }
    if (stmt) {
        sqlite3_finalize(stmt);
    }

    // Current schema version for this binary
    const int targetVersion = 1;

    if (userVersion < targetVersion) {
        // For now we only support migrating from 0 -> 1 by creating all tables.
        if (!createTables()) {
            return false;
        }

        const char* pragma = "PRAGMA user_version = 1;";
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, pragma, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to set user_version: " + std::string(errMsg), "SQLiteHandler");
            sqlite3_free(errMsg);
            metrics.incrementSyncErrors();
            return false;
        }
    } else {
        // Ensure tables exist for current version (idempotent)
        if (!createTables()) {
            return false;
        }
    }

    return true;
}

void SQLiteHandler::shutdown() {
    if (db_) {
        auto& logger = Logger::instance();
        logger.log(LogLevel::INFO, "Closing SQLite database", "SQLiteHandler");
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SQLiteHandler::createTables() {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Creating database tables", "SQLiteHandler");
    
    // ============================================================
    // NORMALIZED SCHEMA (3NF) - Optimized for SentinelFS
    // ============================================================
    // 
    // Design principles:
    // 1. Eliminate redundant file_path storage via file_id FK
    // 2. Lookup tables for op_type, status, threat_type, threat_level
    // 3. Proper indexing on frequently queried columns
    // 4. Foreign key constraints for referential integrity
    // ============================================================
    
    const char* sql = 
        // --- Lookup Tables (Normalization) ---
        "CREATE TABLE IF NOT EXISTS op_types ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE NOT NULL);"  // 'create', 'update', 'delete', 'read', 'write'
        
        "CREATE TABLE IF NOT EXISTS status_types ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE NOT NULL);"  // 'pending', 'syncing', 'completed', 'failed', 'active', 'offline'
        
        "CREATE TABLE IF NOT EXISTS threat_types ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE NOT NULL);"  // 'ransomware', 'malware', 'suspicious', etc.
        
        "CREATE TABLE IF NOT EXISTS threat_levels ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE NOT NULL);"  // 'low', 'medium', 'high', 'critical'
        
        // --- Core Entity Tables ---
        "CREATE TABLE IF NOT EXISTS files ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "path TEXT UNIQUE NOT NULL,"
        "hash TEXT,"
        "timestamp INTEGER,"
        "size INTEGER,"
        "vector_clock TEXT,"
        "synced INTEGER DEFAULT 0);"
        
        "CREATE TABLE IF NOT EXISTS peers ("
        "id TEXT PRIMARY KEY,"
        "address TEXT NOT NULL,"
        "port INTEGER NOT NULL,"
        "last_seen INTEGER,"
        "status_id INTEGER DEFAULT 1,"
        "latency INTEGER DEFAULT -1,"
        "FOREIGN KEY(status_id) REFERENCES status_types(id));"
        
        "CREATE TABLE IF NOT EXISTS device ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "device_id TEXT UNIQUE NOT NULL,"
        "name TEXT,"
        "last_seen INTEGER,"
        "platform TEXT,"
        "version TEXT);"
        
        "CREATE TABLE IF NOT EXISTS config ("
        "key TEXT PRIMARY KEY,"
        "value TEXT);"
        
        // --- Relationship/Log Tables (Normalized with FKs) ---
        "CREATE TABLE IF NOT EXISTS conflicts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_id INTEGER NOT NULL,"
        "local_hash TEXT,"
        "remote_hash TEXT,"
        "remote_peer_id TEXT,"
        "local_timestamp INTEGER,"
        "remote_timestamp INTEGER,"
        "local_size INTEGER,"
        "remote_size INTEGER,"
        "strategy INTEGER,"
        "resolved INTEGER DEFAULT 0,"
        "detected_at INTEGER,"
        "resolved_at INTEGER,"
        "FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE,"
        "FOREIGN KEY(remote_peer_id) REFERENCES peers(id));"

        "CREATE TABLE IF NOT EXISTS session ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "session_id TEXT UNIQUE NOT NULL,"
        "device_id INTEGER NOT NULL,"
        "created_at INTEGER,"
        "last_active INTEGER,"
        "session_code_hash TEXT,"
        "FOREIGN KEY(device_id) REFERENCES device(id) ON DELETE CASCADE);"

        "CREATE TABLE IF NOT EXISTS file_version ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_id INTEGER NOT NULL,"
        "version INTEGER NOT NULL,"
        "hash TEXT,"
        "timestamp INTEGER,"
        "size INTEGER,"
        "device_id INTEGER,"
        "FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE,"
        "FOREIGN KEY(device_id) REFERENCES device(id),"
        "UNIQUE(file_id, version));"

        "CREATE TABLE IF NOT EXISTS sync_queue ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_id INTEGER NOT NULL,"
        "op_type_id INTEGER NOT NULL,"
        "status_id INTEGER NOT NULL,"
        "created_at INTEGER,"
        "last_retry INTEGER,"
        "retry_count INTEGER DEFAULT 0,"
        "FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE,"
        "FOREIGN KEY(op_type_id) REFERENCES op_types(id),"
        "FOREIGN KEY(status_id) REFERENCES status_types(id));"

        "CREATE TABLE IF NOT EXISTS file_access_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_id INTEGER NOT NULL,"
        "op_type_id INTEGER NOT NULL,"
        "device_id INTEGER,"
        "timestamp INTEGER,"
        "FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE,"
        "FOREIGN KEY(op_type_id) REFERENCES op_types(id),"
        "FOREIGN KEY(device_id) REFERENCES device(id));"
        
        "CREATE TABLE IF NOT EXISTS watched_folders ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "path TEXT UNIQUE NOT NULL,"
        "added_at INTEGER,"
        "status_id INTEGER DEFAULT 1,"
        "FOREIGN KEY(status_id) REFERENCES status_types(id));"

        "CREATE TABLE IF NOT EXISTS ignore_patterns ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pattern TEXT UNIQUE NOT NULL,"
        "created_at INTEGER);"
        
        "CREATE TABLE IF NOT EXISTS detected_threats ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_id INTEGER,"
        "file_path TEXT NOT NULL,"
        "threat_type_id INTEGER NOT NULL,"
        "threat_level_id INTEGER NOT NULL,"
        "threat_score REAL NOT NULL,"
        "detected_at TEXT NOT NULL,"
        "entropy REAL,"
        "file_size INTEGER NOT NULL,"
        "hash TEXT,"
        "quarantine_path TEXT,"
        "ml_model_used TEXT,"
        "additional_info TEXT,"
        "marked_safe INTEGER DEFAULT 0,"
        "FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE,"
        "FOREIGN KEY(threat_type_id) REFERENCES threat_types(id),"
        "FOREIGN KEY(threat_level_id) REFERENCES threat_levels(id));"
        
        // --- Indexes for Query Optimization ---
        "CREATE INDEX IF NOT EXISTS idx_files_path ON files(path);"
        "CREATE INDEX IF NOT EXISTS idx_files_hash ON files(hash);"
        "CREATE INDEX IF NOT EXISTS idx_files_synced ON files(synced);"
        "CREATE INDEX IF NOT EXISTS idx_peers_status ON peers(status_id);"
        "CREATE INDEX IF NOT EXISTS idx_peers_latency ON peers(latency);"
        "CREATE INDEX IF NOT EXISTS idx_peers_address_port ON peers(address, port);"
        "CREATE INDEX IF NOT EXISTS idx_conflicts_file ON conflicts(file_id);"
        "CREATE INDEX IF NOT EXISTS idx_conflicts_resolved ON conflicts(resolved);"
        "CREATE INDEX IF NOT EXISTS idx_conflicts_detected ON conflicts(detected_at);"
        "CREATE INDEX IF NOT EXISTS idx_session_device ON session(device_id);"
        "CREATE INDEX IF NOT EXISTS idx_session_active ON session(last_active);"
        "CREATE INDEX IF NOT EXISTS idx_file_version_file ON file_version(file_id);"
        "CREATE INDEX IF NOT EXISTS idx_file_version_timestamp ON file_version(timestamp);"
        "CREATE INDEX IF NOT EXISTS idx_sync_queue_status ON sync_queue(status_id);"
        "CREATE INDEX IF NOT EXISTS idx_sync_queue_file ON sync_queue(file_id);"
        "CREATE INDEX IF NOT EXISTS idx_sync_queue_created ON sync_queue(created_at);"
        "CREATE INDEX IF NOT EXISTS idx_file_access_log_file ON file_access_log(file_id);"
        "CREATE INDEX IF NOT EXISTS idx_file_access_log_timestamp ON file_access_log(timestamp);"
        "CREATE INDEX IF NOT EXISTS idx_device_device_id ON device(device_id);"
        "CREATE INDEX IF NOT EXISTS idx_detected_threats_file ON detected_threats(file_id);"
        "CREATE INDEX IF NOT EXISTS idx_detected_threats_level ON detected_threats(threat_level_id);"
        "CREATE INDEX IF NOT EXISTS idx_detected_threats_detected ON detected_threats(detected_at);";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, 0, 0, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "SQL error: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        return false;
    }
    
    // Enable foreign key enforcement
    if (sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::WARN, "Failed to enable foreign keys: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
    }
    
    // Populate lookup tables with default values
    if (!populateLookupTables()) {
        logger.log(LogLevel::WARN, "Failed to populate lookup tables", "SQLiteHandler");
    }
    
    // Run migrations for backward compatibility
    if (!runMigrations()) {
        logger.log(LogLevel::WARN, "Some migrations failed (non-critical)", "SQLiteHandler");
    }
    
    logger.log(LogLevel::INFO, "Database tables created successfully", "SQLiteHandler");
    return true;
}

bool SQLiteHandler::populateLookupTables() {
    auto& logger = Logger::instance();
    char* errMsg = nullptr;
    
    // Operation types
    const char* opTypesSql = 
        "INSERT OR IGNORE INTO op_types (id, name) VALUES "
        "(1, 'create'), (2, 'update'), (3, 'delete'), (4, 'read'), (5, 'write'), (6, 'rename'), (7, 'move');";
    
    // Status types
    const char* statusTypesSql = 
        "INSERT OR IGNORE INTO status_types (id, name) VALUES "
        "(1, 'active'), (2, 'pending'), (3, 'syncing'), (4, 'completed'), (5, 'failed'), (6, 'offline'), (7, 'paused');";
    
    // Threat types (Zer0 compatible)
    const char* threatTypesSql = 
        "INSERT OR IGNORE INTO threat_types (id, name) VALUES "
        "(0, 'UNKNOWN'), "
        "(1, 'RANSOMWARE_PATTERN'), "
        "(2, 'HIGH_ENTROPY_TEXT'), "
        "(3, 'HIDDEN_EXECUTABLE'), "
        "(4, 'EXTENSION_MISMATCH'), "
        "(5, 'DOUBLE_EXTENSION'), "
        "(6, 'MASS_MODIFICATION'), "
        "(7, 'SCRIPT_IN_DATA'), "
        "(8, 'ANOMALOUS_BEHAVIOR'), "
        "(9, 'KNOWN_MALWARE_HASH'), "
        "(10, 'SUSPICIOUS_RENAME');";
    
    // Threat levels (Zer0 compatible)
    const char* threatLevelsSql = 
        "INSERT OR IGNORE INTO threat_levels (id, name) VALUES "
        "(0, 'NONE'), (1, 'INFO'), (2, 'LOW'), (3, 'MEDIUM'), (4, 'HIGH'), (5, 'CRITICAL');";
    
    bool success = true;
    
    if (sqlite3_exec(db_, opTypesSql, 0, 0, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::WARN, "Failed to populate op_types: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
        success = false;
    }
    
    if (sqlite3_exec(db_, statusTypesSql, 0, 0, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::WARN, "Failed to populate status_types: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
        success = false;
    }
    
    if (sqlite3_exec(db_, threatTypesSql, 0, 0, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::WARN, "Failed to populate threat_types: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
        success = false;
    }
    
    if (sqlite3_exec(db_, threatLevelsSql, 0, 0, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::WARN, "Failed to populate threat_levels: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
        success = false;
    }
    
    return success;
}

bool SQLiteHandler::runMigrations() {
    auto& logger = Logger::instance();
    char* errMsg = nullptr;
    
    // Migration: Add synced column if it doesn't exist (for older databases)
    bool hasSyncedColumn = false;
    sqlite3_stmt* pragmaStmt;
    if (sqlite3_prepare_v2(db_, "PRAGMA table_info(files);", -1, &pragmaStmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(pragmaStmt) == SQLITE_ROW) {
            const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(pragmaStmt, 1));
            if (colName && std::string(colName) == "synced") {
                hasSyncedColumn = true;
                break;
            }
        }
        sqlite3_finalize(pragmaStmt);
    }
    
    if (!hasSyncedColumn) {
        logger.log(LogLevel::INFO, "Adding 'synced' column to files table (migration)", "SQLiteHandler");
        const char* alterSql = "ALTER TABLE files ADD COLUMN synced INTEGER DEFAULT 0;";
        if (sqlite3_exec(db_, alterSql, 0, 0, &errMsg) != SQLITE_OK) {
            logger.log(LogLevel::WARN, "Failed to add synced column (may already exist): " + std::string(errMsg), "SQLiteHandler");
            sqlite3_free(errMsg);
        } else {
            sqlite3_exec(db_, "UPDATE files SET synced = 1;", 0, 0, nullptr);
            logger.log(LogLevel::INFO, "Migration complete: synced column added", "SQLiteHandler");
        }
    }
    
    // One-time migration: Check if synced column migration has been done
    const char* checkMigrationSql = "SELECT value FROM config WHERE key = 'synced_column_migrated';";
    sqlite3_stmt* checkStmt;
    bool migrationDone = false;
    
    if (sqlite3_prepare_v2(db_, checkMigrationSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            migrationDone = true;
        }
        sqlite3_finalize(checkStmt);
    }
    
    if (!migrationDone) {
        const char* migrationSql = "UPDATE files SET synced = 1 WHERE synced IS NULL OR synced = 0;";
        if (sqlite3_exec(db_, migrationSql, 0, 0, &errMsg) == SQLITE_OK) {
            const char* markDoneSql = "INSERT OR REPLACE INTO config (key, value) VALUES ('synced_column_migrated', '1');";
            sqlite3_exec(db_, markDoneSql, 0, 0, nullptr);
            logger.log(LogLevel::INFO, "Migrated existing files to synced status", "SQLiteHandler");
        } else {
            logger.log(LogLevel::WARN, "Migration warning (non-critical): " + std::string(errMsg), "SQLiteHandler");
            sqlite3_free(errMsg);
        }
    }
    
    // Schema version 2 migration: Migrate old TEXT-based columns to FK-based
    const char* checkV2Sql = "SELECT value FROM config WHERE key = 'schema_version';";
    sqlite3_stmt* v2Stmt;
    int schemaVersion = 0;
    
    if (sqlite3_prepare_v2(db_, checkV2Sql, -1, &v2Stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(v2Stmt) == SQLITE_ROW) {
            schemaVersion = std::stoi(reinterpret_cast<const char*>(sqlite3_column_text(v2Stmt, 0)));
        }
        sqlite3_finalize(v2Stmt);
    }
    
    if (schemaVersion < 2) {
        // Mark schema as version 2
        const char* setVersionSql = "INSERT OR REPLACE INTO config (key, value) VALUES ('schema_version', '2');";
        sqlite3_exec(db_, setVersionSql, 0, 0, nullptr);
        logger.log(LogLevel::INFO, "Schema upgraded to version 2 (normalized)", "SQLiteHandler");
    }
    
    return true;
}

} // namespace SentinelFS
