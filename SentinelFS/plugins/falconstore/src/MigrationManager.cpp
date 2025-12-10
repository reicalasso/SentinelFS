/**
 * @file MigrationManager.cpp
 * @brief Migration manager implementation
 */

#include "MigrationManager.h"
#include "Logger.h"

namespace SentinelFS {
namespace Falcon {

MigrationManager::MigrationManager(sqlite3* db) : db_(db) {
    ensureMigrationTable();
}

void MigrationManager::registerMigration(const Migration& migration) {
    migrations_[migration.version] = migration;
}

int MigrationManager::getCurrentVersion() const {
    const char* sql = "SELECT version FROM schema_migrations ORDER BY version DESC LIMIT 1";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return version;
}

int MigrationManager::getLatestVersion() const {
    if (migrations_.empty()) return 0;
    return migrations_.rbegin()->first;
}

bool MigrationManager::migrateUp(int targetVersion) {
    auto& logger = Logger::instance();
    
    int currentVersion = getCurrentVersion();
    int target = (targetVersion < 0) ? getLatestVersion() : targetVersion;
    
    if (currentVersion >= target) {
        logger.log(LogLevel::DEBUG, "Schema is up to date (v" + std::to_string(currentVersion) + ")", "Migration");
        return true;
    }
    
    logger.log(LogLevel::INFO, "Migrating from v" + std::to_string(currentVersion) + 
               " to v" + std::to_string(target), "Migration");
    
    // Execute migrations in order
    for (const auto& [version, migration] : migrations_) {
        if (version <= currentVersion) continue;
        if (version > target) break;
        
        logger.log(LogLevel::INFO, "Running migration v" + std::to_string(version) + 
                   ": " + migration.name, "Migration");
        
        if (!executeMigration(migration, true)) {
            logger.log(LogLevel::ERROR, "Migration v" + std::to_string(version) + " failed", "Migration");
            return false;
        }
        
        if (!setVersion(version)) {
            logger.log(LogLevel::ERROR, "Failed to update schema version", "Migration");
            return false;
        }
    }
    
    logger.log(LogLevel::INFO, "Migration complete. Schema version: " + std::to_string(target), "Migration");
    return true;
}

bool MigrationManager::migrateDown(int targetVersion) {
    auto& logger = Logger::instance();
    
    int currentVersion = getCurrentVersion();
    
    if (currentVersion <= targetVersion) {
        logger.log(LogLevel::DEBUG, "Schema is already at or below target version", "Migration");
        return true;
    }
    
    logger.log(LogLevel::INFO, "Rolling back from v" + std::to_string(currentVersion) + 
               " to v" + std::to_string(targetVersion), "Migration");
    
    // Execute rollbacks in reverse order
    for (auto it = migrations_.rbegin(); it != migrations_.rend(); ++it) {
        int version = it->first;
        const Migration& migration = it->second;
        
        if (version > currentVersion) continue;
        if (version <= targetVersion) break;
        
        logger.log(LogLevel::INFO, "Rolling back migration v" + std::to_string(version) + 
                   ": " + migration.name, "Migration");
        
        if (!executeMigration(migration, false)) {
            logger.log(LogLevel::ERROR, "Rollback v" + std::to_string(version) + " failed", "Migration");
            return false;
        }
        
        // Remove version record
        const char* sql = "DELETE FROM schema_migrations WHERE version = ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, version);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    logger.log(LogLevel::INFO, "Rollback complete. Schema version: " + std::to_string(targetVersion), "Migration");
    return true;
}

std::vector<Migration> MigrationManager::getPendingMigrations() const {
    std::vector<Migration> pending;
    int currentVersion = getCurrentVersion();
    
    for (const auto& [version, migration] : migrations_) {
        if (version > currentVersion) {
            pending.push_back(migration);
        }
    }
    
    return pending;
}

bool MigrationManager::ensureMigrationTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            applied_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            Logger::instance().log(LogLevel::ERROR, "Failed to create migrations table: " + 
                                   std::string(errMsg), "Migration");
            sqlite3_free(errMsg);
        }
        return false;
    }
    
    return true;
}

bool MigrationManager::setVersion(int version) {
    auto it = migrations_.find(version);
    if (it == migrations_.end()) return false;
    
    const char* sql = "INSERT INTO schema_migrations (version, name) VALUES (?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, version);
    sqlite3_bind_text(stmt, 2, it->second.name.c_str(), -1, SQLITE_TRANSIENT);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    return success;
}

bool MigrationManager::executeMigration(const Migration& migration, bool up) {
    auto& logger = Logger::instance();
    
    // Start transaction
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            logger.log(LogLevel::ERROR, "Failed to begin transaction: " + std::string(errMsg), "Migration");
            sqlite3_free(errMsg);
        }
        return false;
    }
    
    bool success = true;
    
    // Execute SQL
    const std::string& sql = up ? migration.upSql : migration.downSql;
    if (!sql.empty()) {
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            if (errMsg) {
                logger.log(LogLevel::ERROR, "Migration SQL failed: " + std::string(errMsg), "Migration");
                sqlite3_free(errMsg);
            }
            success = false;
        }
    }
    
    // Execute callback if provided
    if (success) {
        const auto& callback = up ? migration.upCallback : migration.downCallback;
        if (callback) {
            success = callback(db_);
        }
    }
    
    // Commit or rollback
    if (success) {
        if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            if (errMsg) {
                logger.log(LogLevel::ERROR, "Failed to commit: " + std::string(errMsg), "Migration");
                sqlite3_free(errMsg);
            }
            success = false;
        }
    }
    
    if (!success) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    
    return success;
}

} // namespace Falcon
} // namespace SentinelFS
