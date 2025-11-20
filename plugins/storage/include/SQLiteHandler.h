#pragma once

#include <sqlite3.h>
#include <string>

namespace SentinelFS {

/**
 * @brief Handles SQLite database connection and schema management
 */
class SQLiteHandler {
public:
    SQLiteHandler() = default;
    ~SQLiteHandler();

    // Prevent copying
    SQLiteHandler(const SQLiteHandler&) = delete;
    SQLiteHandler& operator=(const SQLiteHandler&) = delete;

    /**
     * @brief Open database connection and create tables
     */
    bool initialize(const std::string& dbPath = "sentinel.db");

    /**
     * @brief Close database connection
     */
    void shutdown();

    /**
     * @brief Get database handle
     */
    sqlite3* getDB() { return db_; }

private:
    sqlite3* db_ = nullptr;

    /**
     * @brief Create all required database tables
     */
    bool createTables();
};

} // namespace SentinelFS
