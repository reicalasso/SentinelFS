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

        const char* sql = "INSERT OR REPLACE INTO session (session_id, device_id, created_at, last_active, session_code_hash) "
                          "VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        sqlite3* db = handler_->getDB();

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "SessionManager");
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, deviceId.c_str(), -1, SQLITE_STATIC);
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
