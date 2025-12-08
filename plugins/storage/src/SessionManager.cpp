#include "SessionManager.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    bool SessionManager::upsertSession(const std::string& sessionId,
                                       const std::string& deviceId,
                                       long long createdAt,
                                       long long lastActive,
                                       const std::string& sessionCodeHash) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        sqlite3* db = handler_->getDB();
        
        // Get or create device_id (integer) from device table
        int deviceDbId = 0;
        const char* getDeviceIdSql = "SELECT id FROM device WHERE device_id = ?;";
        sqlite3_stmt* deviceStmt;
        if (sqlite3_prepare_v2(db, getDeviceIdSql, -1, &deviceStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(deviceStmt, 1, deviceId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(deviceStmt) == SQLITE_ROW) {
                deviceDbId = sqlite3_column_int(deviceStmt, 0);
            }
            sqlite3_finalize(deviceStmt);
        }
        
        // If device doesn't exist, create it first
        if (deviceDbId == 0) {
            const char* insertDeviceSql = "INSERT INTO device (device_id, last_seen) VALUES (?, ?);";
            sqlite3_stmt* insertStmt;
            if (sqlite3_prepare_v2(db, insertDeviceSql, -1, &insertStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(insertStmt, 1, deviceId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int64(insertStmt, 2, lastActive);
                if (sqlite3_step(insertStmt) == SQLITE_DONE) {
                    deviceDbId = static_cast<int>(sqlite3_last_insert_rowid(db));
                }
                sqlite3_finalize(insertStmt);
            }
        }
        
        if (deviceDbId == 0) {
            logger.log(LogLevel::ERROR, "Failed to get or create device_id for: " + deviceId, "SessionManager");
            metrics.incrementSyncErrors();
            return false;
        }

        const char* sql = "INSERT OR REPLACE INTO session (session_id, device_id, created_at, last_active, session_code_hash) "
                          "VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "SessionManager");
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, deviceDbId);
        sqlite3_bind_int64(stmt, 3, createdAt);
        sqlite3_bind_int64(stmt, 4, lastActive);
        sqlite3_bind_text(stmt, 5, sessionCodeHash.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "SessionManager");
            sqlite3_finalize(stmt);
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

    bool SessionManager::touchSession(const std::string& sessionId, long long lastActive) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();

        const char* sql = "UPDATE session SET last_active = ? WHERE session_id = ?;";
        sqlite3_stmt* stmt;
        sqlite3* db = handler_->getDB();

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "SessionManager");
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_bind_int64(stmt, 1, lastActive);
        sqlite3_bind_text(stmt, 2, sessionId.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "SessionManager");
            sqlite3_finalize(stmt);
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

} // namespace SentinelFS
