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
            logger.log(LogLevel::ERROR, "Failed to get or create file_id for: " + filePath, "FileAccessLogManager");
            metrics.incrementSyncErrors();
            return false;
        }
        
        // Map opType string to op_type_id
        int opTypeId = 4; // default: read
        if (opType == "create") opTypeId = 1;
        else if (opType == "update") opTypeId = 2;
        else if (opType == "delete") opTypeId = 3;
        else if (opType == "write") opTypeId = 5;
        else if (opType == "rename") opTypeId = 6;
        else if (opType == "move") opTypeId = 7;
        
        // Get device_id (integer) from device table if deviceId string is provided
        int deviceDbId = 0;
        if (!deviceId.empty()) {
            const char* getDeviceIdSql = "SELECT id FROM device WHERE device_id = ?;";
            sqlite3_stmt* deviceStmt;
            if (sqlite3_prepare_v2(db, getDeviceIdSql, -1, &deviceStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(deviceStmt, 1, deviceId.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(deviceStmt) == SQLITE_ROW) {
                    deviceDbId = sqlite3_column_int(deviceStmt, 0);
                }
                sqlite3_finalize(deviceStmt);
            }
        }

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
