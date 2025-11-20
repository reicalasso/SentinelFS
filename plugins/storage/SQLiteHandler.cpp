#include "SQLiteHandler.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>

namespace SentinelFS {

SQLiteHandler::~SQLiteHandler() {
    shutdown();
}

bool SQLiteHandler::initialize(const std::string& dbPath) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::INFO, "Initializing SQLite database: " + dbPath, "SQLiteHandler");
    
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Cannot open database: " + std::string(sqlite3_errmsg(db_)), "SQLiteHandler");
        metrics.incrementSyncErrors();
        return false;
    }
    
    logger.log(LogLevel::INFO, "Database opened successfully", "SQLiteHandler");
    return createTables();
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
        "resolved_at INTEGER);";

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
