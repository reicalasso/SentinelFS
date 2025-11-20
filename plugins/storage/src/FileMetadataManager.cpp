#include "FileMetadataManager.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>

namespace SentinelFS {

bool FileMetadataManager::addFile(const std::string& path, const std::string& hash, 
                                   long long timestamp, long long size) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Adding file metadata: " + path, "FileMetadataManager");
    
    const char* sql = "INSERT OR REPLACE INTO files (path, hash, timestamp, size) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    bool inTransaction = false;
    char* errMsg = nullptr;

    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to begin transaction: " + std::string(errMsg), "FileMetadataManager");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        return false;
    }
    inTransaction = true;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "FileMetadataManager");
        metrics.incrementSyncErrors();
        if (inTransaction) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
        return false;
    }

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, timestamp);
    sqlite3_bind_int64(stmt, 4, size);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "FileMetadataManager");
        sqlite3_finalize(stmt);
        metrics.incrementSyncErrors();
        if (inTransaction) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
        return false;
    }

    sqlite3_finalize(stmt);

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to commit transaction: " + std::string(errMsg), "FileMetadataManager");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    metrics.incrementFilesWatched();
    return true;
}

std::optional<FileMetadata> FileMetadataManager::getFile(const std::string& path) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    const char* sql = "SELECT path, hash, timestamp, size FROM files WHERE path = ?;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "FileMetadataManager");
        metrics.incrementSyncErrors();
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
        logger.log(LogLevel::DEBUG, "Retrieved file metadata: " + path, "FileMetadataManager");
    }

    sqlite3_finalize(stmt);
    return result;
}

bool FileMetadataManager::removeFile(const std::string& path) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Removing file metadata: " + path, "FileMetadataManager");
    
    const char* sql = "DELETE FROM files WHERE path = ?;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    bool inTransaction = false;
    char* errMsg = nullptr;

    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to begin transaction: " + std::string(errMsg), "FileMetadataManager");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        return false;
    }
    inTransaction = true;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "FileMetadataManager");
        metrics.incrementSyncErrors();
        if (inTransaction) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
        return false;
    }

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "FileMetadataManager");
        sqlite3_finalize(stmt);
        metrics.incrementSyncErrors();
        if (inTransaction) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
        return false;
    }

    sqlite3_finalize(stmt);

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        logger.log(LogLevel::ERROR, "Failed to commit transaction: " + std::string(errMsg), "FileMetadataManager");
        sqlite3_free(errMsg);
        metrics.incrementSyncErrors();
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    metrics.incrementFilesDeleted();
    logger.log(LogLevel::INFO, "File metadata removed: " + path, "FileMetadataManager");
    return true;
}

} // namespace SentinelFS
