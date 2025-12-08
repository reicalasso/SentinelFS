#pragma once

#include <sqlite3.h>
#include <string>
#include <unordered_map>

namespace SentinelFS {

/**
 * @brief Common database helper functions to eliminate code duplication
 * 
 * Provides:
 * - File ID lookup/creation
 * - Operation type mapping
 * - Status type mapping
 * - Device ID lookup
 */
class DBHelper {
public:
    // Operation type IDs (matches op_types lookup table)
    enum class OpType : int {
        CREATE = 1,
        UPDATE = 2,
        DELETE = 3,
        READ = 4,
        WRITE = 5,
        RENAME = 6,
        MOVE = 7
    };
    
    // Status type IDs (matches status_types lookup table)
    enum class StatusType : int {
        ACTIVE = 1,
        PENDING = 2,
        SYNCING = 3,
        COMPLETED = 4,
        FAILED = 5,
        OFFLINE = 6,
        PAUSED = 7
    };
    
    // Threat type IDs (matches threat_types lookup table)
    enum class ThreatType : int {
        RANSOMWARE = 1,
        MALWARE = 2,
        SUSPICIOUS = 3,
        ENTROPY_ANOMALY = 4,
        RAPID_MODIFICATION = 5,
        MASS_DELETION = 6
    };
    
    // Threat level IDs (matches threat_levels lookup table)
    enum class ThreatLevel : int {
        LOW = 1,
        MEDIUM = 2,
        HIGH = 3,
        CRITICAL = 4
    };

    /**
     * @brief Get file ID from path, creating if not exists
     * @param db SQLite database handle
     * @param path File path
     * @return File ID or 0 on failure
     */
    static int getOrCreateFileId(sqlite3* db, const std::string& path);
    
    /**
     * @brief Get file ID from path (no creation)
     * @param db SQLite database handle
     * @param path File path
     * @return File ID or 0 if not found
     */
    static int getFileId(sqlite3* db, const std::string& path);
    
    /**
     * @brief Get device ID from device_id string, creating if not exists
     * @param db SQLite database handle
     * @param deviceId Device identifier string
     * @return Device table ID or 0 on failure
     */
    static int getOrCreateDeviceId(sqlite3* db, const std::string& deviceId);
    
    /**
     * @brief Get device ID from device_id string (no creation)
     * @param db SQLite database handle
     * @param deviceId Device identifier string
     * @return Device table ID or 0 if not found
     */
    static int getDeviceId(sqlite3* db, const std::string& deviceId);
    
    /**
     * @brief Map operation type string to ID
     * @param opType Operation type string (create, update, delete, read, write, rename, move)
     * @return OpType enum value
     */
    static OpType mapOpType(const std::string& opType);
    
    /**
     * @brief Map status string to ID
     * @param status Status string (active, pending, syncing, completed, failed, offline, paused)
     * @return StatusType enum value
     */
    static StatusType mapStatus(const std::string& status);
    
    /**
     * @brief Map threat type string to ID
     * @param threatType Threat type string
     * @return ThreatType enum value
     */
    static ThreatType mapThreatType(const std::string& threatType);
    
    /**
     * @brief Map threat level string to ID
     * @param threatLevel Threat level string (low, medium, high, critical)
     * @return ThreatLevel enum value
     */
    static ThreatLevel mapThreatLevel(const std::string& threatLevel);
};

} // namespace SentinelFS
