#include "FileAccessLogManager.h"
#include "DBHelper.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    bool FileAccessLogManager::logAccess(const std::string& filePath,
                                         const std::string& opType,
                                         const std::string& deviceId,
                                         long long timestamp) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        sqlite3* db = handler_->getDB();
        
        int fileId = DBHelper::getOrCreateFileId(db, filePath);
        if (fileId == 0) {
            logger.log(LogLevel::ERROR, "Failed to get or create file_id for: " + filePath, "FileAccessLogManager");
            metrics.incrementSyncErrors();
            return false;
        }
        
        int opTypeId = static_cast<int>(DBHelper::mapOpType(opType));
        int deviceDbId = DBHelper::getDeviceId(db, deviceId);

        const char* sql = "INSERT INTO file_access_log (file_id, op_type_id, device_id, timestamp) "
                          "VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "FileAccessLogManager");
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_bind_int(stmt, 1, fileId);
        sqlite3_bind_int(stmt, 2, opTypeId);
        if (deviceDbId > 0) {
            sqlite3_bind_int(stmt, 3, deviceDbId);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
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
