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
    
    const char* sql = 
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
        "strategy INTEGER,"
        "resolved INTEGER DEFAULT 0,"
        "detected_at INTEGER,"
        "resolved_at INTEGER);"

        "CREATE TABLE IF NOT EXISTS device ("
        "device_id TEXT PRIMARY KEY,"
        "name TEXT,"
        "last_seen INTEGER,"
        "platform TEXT,"
        "version TEXT);"

        "CREATE TABLE IF NOT EXISTS session ("
        "session_id TEXT PRIMARY KEY,"
        "device_id TEXT NOT NULL,"
        "created_at INTEGER,"
        "last_active INTEGER,"
        "session_code_hash TEXT,"
        "FOREIGN KEY(device_id) REFERENCES device(device_id));"

        "CREATE TABLE IF NOT EXISTS file_version ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_path TEXT NOT NULL,"
        "version INTEGER,"
        "hash TEXT,"
        "timestamp INTEGER,"
        "size INTEGER,"
        "device_id TEXT,"
        "FOREIGN KEY(device_id) REFERENCES device(device_id));"

        "CREATE TABLE IF NOT EXISTS sync_queue ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_path TEXT NOT NULL,"
        "op_type TEXT NOT NULL,"
        "status TEXT NOT NULL,"
        "created_at INTEGER,"
        "last_retry INTEGER,"
        "retry_count INTEGER DEFAULT 0);"

        "CREATE TABLE IF NOT EXISTS file_access_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_path TEXT NOT NULL,"
        "op_type TEXT NOT NULL,"
        "device_id TEXT,"
        "timestamp INTEGER,"
        "FOREIGN KEY(device_id) REFERENCES device(device_id));"
        
        "CREATE TABLE IF NOT EXISTS watched_folders ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "path TEXT UNIQUE NOT NULL,"
        "added_at INTEGER,"
        "status TEXT DEFAULT 'active');"

        "CREATE TABLE IF NOT EXISTS ignore_patterns ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pattern TEXT UNIQUE NOT NULL,"
        "created_at INTEGER);"
        
        "CREATE TABLE IF NOT EXISTS detected_threats ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_path TEXT NOT NULL,"
        "threat_type TEXT NOT NULL,"
        "threat_level TEXT NOT NULL,"
        "threat_score REAL NOT NULL,"
        "detected_at INTEGER NOT NULL,"
        "entropy REAL,"
        "file_size INTEGER NOT NULL,"
        "hash TEXT,"
        "quarantine_path TEXT,"
        "ml_model_used TEXT,"
        "additional_info TEXT,"
        "marked_safe INTEGER DEFAULT 0);";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, 0, 0, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "SQL error: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        return false;
    }
    
    // Migration: Add synced column if it doesn't exist (for older databases)
    // SQLite doesn't support IF NOT EXISTS for ALTER TABLE, so we check first
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
            // Mark all existing files as synced since they were tracked before this feature
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
        // First time after adding synced column - mark all existing files as synced
        const char* migrationSql = "UPDATE files SET synced = 1 WHERE synced IS NULL OR synced = 0;";
        if (sqlite3_exec(db_, migrationSql, 0, 0, &errMsg) == SQLITE_OK) {
            // Mark migration as done
            const char* markDoneSql = "INSERT OR REPLACE INTO config (key, value) VALUES ('synced_column_migrated', '1');";
            sqlite3_exec(db_, markDoneSql, 0, 0, nullptr);
            logger.log(LogLevel::INFO, "Migrated existing files to synced status", "SQLiteHandler");
        } else {
            logger.log(LogLevel::WARN, "Migration warning (non-critical): " + std::string(errMsg), "SQLiteHandler");
            sqlite3_free(errMsg);
        }
    }
    
    logger.log(LogLevel::INFO, "Database tables created successfully", "SQLiteHandler");
    return true;
}

} // namespace SentinelFS
