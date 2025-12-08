#include "DBHelper.h"
#include <algorithm>
#include <cctype>

namespace SentinelFS {

int DBHelper::getOrCreateFileId(sqlite3* db, const std::string& path) {
    int fileId = getFileId(db, path);
    if (fileId > 0) {
        return fileId;
    }
    
    // Create new file entry
    const char* insertSql = "INSERT INTO files (path) VALUES (?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            fileId = static_cast<int>(sqlite3_last_insert_rowid(db));
        }
        sqlite3_finalize(stmt);
    }
    
    return fileId;
}

int DBHelper::getFileId(sqlite3* db, const std::string& path) {
    int fileId = 0;
    const char* sql = "SELECT id FROM files WHERE path = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fileId = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    return fileId;
}

int DBHelper::getOrCreateDeviceId(sqlite3* db, const std::string& deviceId) {
    if (deviceId.empty()) {
        return 0;
    }
    
    int dbId = getDeviceId(db, deviceId);
    if (dbId > 0) {
        return dbId;
    }
    
    // Create new device entry
    const char* insertSql = "INSERT INTO device (device_id) VALUES (?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, deviceId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            dbId = static_cast<int>(sqlite3_last_insert_rowid(db));
        }
        sqlite3_finalize(stmt);
    }
    
    return dbId;
}

int DBHelper::getDeviceId(sqlite3* db, const std::string& deviceId) {
    if (deviceId.empty()) {
        return 0;
    }
    
    int dbId = 0;
    const char* sql = "SELECT id FROM device WHERE device_id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, deviceId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            dbId = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    return dbId;
}

DBHelper::OpType DBHelper::mapOpType(const std::string& opType) {
    std::string lower = opType;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower == "create") return OpType::CREATE;
    if (lower == "update") return OpType::UPDATE;
    if (lower == "delete") return OpType::DELETE;
    if (lower == "read") return OpType::READ;
    if (lower == "write") return OpType::WRITE;
    if (lower == "rename") return OpType::RENAME;
    if (lower == "move") return OpType::MOVE;
    
    return OpType::CREATE; // default
}

DBHelper::StatusType DBHelper::mapStatus(const std::string& status) {
    std::string lower = status;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower == "active") return StatusType::ACTIVE;
    if (lower == "pending") return StatusType::PENDING;
    if (lower == "syncing") return StatusType::SYNCING;
    if (lower == "completed") return StatusType::COMPLETED;
    if (lower == "failed") return StatusType::FAILED;
    if (lower == "offline") return StatusType::OFFLINE;
    if (lower == "paused") return StatusType::PAUSED;
    
    return StatusType::PENDING; // default
}

DBHelper::ThreatType DBHelper::mapThreatType(const std::string& threatType) {
    std::string lower = threatType;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower == "ransomware") return ThreatType::RANSOMWARE;
    if (lower == "malware") return ThreatType::MALWARE;
    if (lower == "suspicious") return ThreatType::SUSPICIOUS;
    if (lower == "entropy_anomaly") return ThreatType::ENTROPY_ANOMALY;
    if (lower == "rapid_modification") return ThreatType::RAPID_MODIFICATION;
    if (lower == "mass_deletion") return ThreatType::MASS_DELETION;
    
    return ThreatType::SUSPICIOUS; // default
}

DBHelper::ThreatLevel DBHelper::mapThreatLevel(const std::string& threatLevel) {
    std::string lower = threatLevel;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower == "low") return ThreatLevel::LOW;
    if (lower == "medium") return ThreatLevel::MEDIUM;
    if (lower == "high") return ThreatLevel::HIGH;
    if (lower == "critical") return ThreatLevel::CRITICAL;
    
    return ThreatLevel::LOW; // default
}

} // namespace SentinelFS
