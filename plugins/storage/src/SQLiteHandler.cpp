#include "SQLiteHandler.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <cstdlib>

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
            // Force usage of 'data' directory
            resolvedPath = "data/sentinel.db";
        }
    }

    // Ensure directory exists using system command (fallback for filesystem issues)
    std::string dirPath;
    size_t lastSlash = resolvedPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        dirPath = resolvedPath.substr(0, lastSlash);
    }

    if (!dirPath.empty()) {
        std::string command = "mkdir -p \"" + dirPath + "\"";
        int res = std::system(command.c_str());
        if (res != 0) {
             logger.log(LogLevel::ERROR, "Failed to create database directory: " + dirPath, "SQLiteHandler");
        }
    }

    logger.log(LogLevel::INFO, "Initializing SQLite database: " + resolvedPath, "SQLiteHandler");

    if (sqlite3_open(resolvedPath.c_str(), &db_) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Cannot open database: " + std::string(sqlite3_errmsg(db_)), "SQLiteHandler");
        metrics.incrementSyncErrors();
        return false;
    }

    logger.log(LogLevel::INFO, "Database opened successfully", "SQLiteHandler");

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
        "vector_clock TEXT);"
        
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
        "status TEXT DEFAULT 'active');";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, 0, 0, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "SQL error: " + std::string(errMsg), "SQLiteHandler");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        return false;
    }
    
    logger.log(LogLevel::INFO, "Database tables created successfully", "SQLiteHandler");
    return true;
}

} // namespace SentinelFS
