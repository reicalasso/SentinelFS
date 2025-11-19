#pragma once

#include "IStorageAPI.h"
#include "SQLiteHandler.h"
#include <vector>
#include <utility>

namespace SentinelFS {

/**
 * @brief Manages conflict detection and resolution tracking
 */
class ConflictManager {
public:
    explicit ConflictManager(SQLiteHandler* handler) : handler_(handler) {}

    /**
     * @brief Record a new conflict
     */
    bool addConflict(const ConflictInfo& conflict);

    /**
     * @brief Get all unresolved conflicts
     */
    std::vector<ConflictInfo> getUnresolvedConflicts();

    /**
     * @brief Get conflicts for specific file
     */
    std::vector<ConflictInfo> getConflictsForFile(const std::string& path);

    /**
     * @brief Mark conflict as resolved
     */
    bool markConflictResolved(int conflictId);

    /**
     * @brief Get conflict statistics (total, unresolved)
     */
    std::pair<int, int> getConflictStats();

private:
    SQLiteHandler* handler_;

    /**
     * @brief Execute query and return conflicts
     */
    std::vector<ConflictInfo> queryConflicts(const char* sql, sqlite3_stmt* stmt = nullptr);

    /**
     * @brief Parse ConflictInfo from SQL row
     */
    ConflictInfo parseConflictRow(sqlite3_stmt* stmt);
};

} // namespace SentinelFS
