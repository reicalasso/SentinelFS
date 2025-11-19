#include "FileMetadataManager.h"
#include <iostream>

namespace SentinelFS {

bool FileMetadataManager::addFile(const std::string& path, const std::string& hash, 
                                   long long timestamp, long long size) {
    const char* sql = "INSERT OR REPLACE INTO files (path, hash, timestamp, size) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, timestamp);
    sqlite3_bind_int64(stmt, 4, size);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

std::optional<FileMetadata> FileMetadataManager::getFile(const std::string& path) {
    const char* sql = "SELECT path, hash, timestamp, size FROM files WHERE path = ?;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
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

bool FileMetadataManager::removeFile(const std::string& path) {
    const char* sql = "DELETE FROM files WHERE path = ?;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler_->getDB();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

} // namespace SentinelFS
