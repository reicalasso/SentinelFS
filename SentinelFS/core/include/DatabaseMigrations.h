#pragma once

#include "DatabaseManager.h"
#include <vector>

namespace SentinelFS {

/**
 * @brief Database schema migrations for SentinelFS
 */
class DatabaseMigrations {
public:
    /**
     * @brief Get all available migrations in order
     * @return Vector of migrations
     */
    static std::vector<DatabaseManager::Migration> getAllMigrations();
    
    /**
     * @brief Get initial schema (version 1)
     * @return SQL for initial schema creation
     */
    static std::string getInitialSchema();
    
private:
    // Individual migration methods
    static DatabaseManager::Migration migration_v1();
    static DatabaseManager::Migration migration_v2();
    static DatabaseManager::Migration migration_v3();
    static DatabaseManager::Migration migration_v4();
    static DatabaseManager::Migration migration_v5();
    static DatabaseManager::Migration migration_v6();
};

} // namespace SentinelFS
