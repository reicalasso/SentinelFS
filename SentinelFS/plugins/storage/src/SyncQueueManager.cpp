#include "SyncQueueManager.h"
#include "DBHelper.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    bool SyncQueueManager::enqueue(const std::string& filePath,
                                   const std::string& opType,
                                   const std::string& status) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        sqlite3* db = handler_->getDB();
        
        int fileId = DBHelper::getOrCreateFileId(db, filePath);
        if (fileId == 0) {
            logger.log(LogLevel::ERROR, "Failed to get or create file_id for: " + filePath, "SyncQueueManager");
            metrics.incrementSyncErrors();
            return false;
        }
        
        int opTypeId = static_cast<int>(DBHelper::mapOpType(opType));
        int statusId = static_cast<int>(DBHelper::mapStatus(status));

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
