#include "SyncQueueManager.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    bool SyncQueueManager::enqueue(const std::string& filePath,
                                   const std::string& opType,
                                   const std::string& status) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        sqlite3* db = handler_->getDB();
        
        // Get or create file_id
        int fileId = 0;
        const char* getFileIdSql = "SELECT id FROM files WHERE path = ?;";
        sqlite3_stmt* fileStmt;
        if (sqlite3_prepare_v2(db, getFileIdSql, -1, &fileStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(fileStmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(fileStmt) == SQLITE_ROW) {
                fileId = sqlite3_column_int(fileStmt, 0);
            }
            sqlite3_finalize(fileStmt);
        }
        
        // If file doesn't exist, create it first
        if (fileId == 0) {
            const char* insertFileSql = "INSERT INTO files (path) VALUES (?);";
            sqlite3_stmt* insertStmt;
            if (sqlite3_prepare_v2(db, insertFileSql, -1, &insertStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(insertStmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(insertStmt) == SQLITE_DONE) {
                    fileId = static_cast<int>(sqlite3_last_insert_rowid(db));
                }
                sqlite3_finalize(insertStmt);
            }
        }
        
        if (fileId == 0) {
            logger.log(LogLevel::ERROR, "Failed to get or create file_id for: " + filePath, "SyncQueueManager");
            metrics.incrementSyncErrors();
            return false;
        }
        
        // Map opType string to op_type_id
        int opTypeId = 1; // default: create
        if (opType == "update") opTypeId = 2;
        else if (opType == "delete") opTypeId = 3;
        else if (opType == "read") opTypeId = 4;
        else if (opType == "write") opTypeId = 5;
        else if (opType == "rename") opTypeId = 6;
        else if (opType == "move") opTypeId = 7;
        
        // Map status string to status_id
        int statusId = 2; // default: pending
        if (status == "active") statusId = 1;
        else if (status == "syncing") statusId = 3;
        else if (status == "completed") statusId = 4;
        else if (status == "failed") statusId = 5;

        const char* sql = "INSERT INTO sync_queue (file_id, op_type_id, status_id, created_at, last_retry, retry_count) "
                          "VALUES (?, ?, ?, strftime('%s','now'), 0, 0);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "SyncQueueManager");
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_bind_int(stmt, 1, fileId);
        sqlite3_bind_int(stmt, 2, opTypeId);
        sqlite3_bind_int(stmt, 3, statusId);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "SyncQueueManager");
            sqlite3_finalize(stmt);
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

} // namespace SentinelFS
