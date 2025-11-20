#include "SyncQueueManager.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    bool SyncQueueManager::enqueue(const std::string& filePath,
                                   const std::string& opType,
                                   const std::string& status) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();

        const char* sql = "INSERT INTO sync_queue (file_path, op_type, status, created_at, last_retry, retry_count) "
                          "VALUES (?, ?, ?, strftime('%s','now'), 0, 0);";
        sqlite3_stmt* stmt;
        sqlite3* db = handler_->getDB();

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "SyncQueueManager");
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, opType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_STATIC);

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
