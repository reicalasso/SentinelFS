#include "DeviceManager.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    bool DeviceManager::upsertDevice(const std::string& deviceId,
                                     const std::string& name,
                                     long long lastSeen,
                                     const std::string& platform,
                                     const std::string& version) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        sqlite3* db = handler_->getDB();

        // Use INSERT ... ON CONFLICT for proper upsert with AUTOINCREMENT id
        const char* sql = "INSERT INTO device (device_id, name, last_seen, platform, version) "
                          "VALUES (?, ?, ?, ?, ?) "
                          "ON CONFLICT(device_id) DO UPDATE SET "
                          "name = excluded.name, last_seen = excluded.last_seen, "
                          "platform = excluded.platform, version = excluded.version;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "DeviceManager");
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_bind_text(stmt, 1, deviceId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, lastSeen);
        sqlite3_bind_text(stmt, 4, platform.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, version.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            logger.log(LogLevel::ERROR, "Failed to execute statement: " + std::string(sqlite3_errmsg(db)), "DeviceManager");
            sqlite3_finalize(stmt);
            metrics.incrementSyncErrors();
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

    std::vector<std::string> DeviceManager::getAllDeviceIds() {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();

        std::vector<std::string> ids;
        const char* sql = "SELECT device_id FROM device;";
        sqlite3_stmt* stmt;
        sqlite3* db = handler_->getDB();

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger.log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db)), "DeviceManager");
            metrics.incrementSyncErrors();
            return ids;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (id) {
                ids.emplace_back(id);
            }
        }

        sqlite3_finalize(stmt);
        return ids;
    }

} // namespace SentinelFS
