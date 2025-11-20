#include "FileAccessLogManager.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    bool FileAccessLogManager::logAccess(const std::string& filePath,
                                         const std::string& opType,
                                         const std::string& deviceId,
                                         long long timestamp) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();

        const char* sql = "INSERT INTO file_access_log (file_path, op_type, device_id, timestamp) "
                          "VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        sqlite3* db = handler_->getDB();

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "FileAccessLogManager");
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, opType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, deviceId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, timestamp);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "FileAccessLogManager");
            sqlite3_finalize(stmt);
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

} // namespace SentinelFS
