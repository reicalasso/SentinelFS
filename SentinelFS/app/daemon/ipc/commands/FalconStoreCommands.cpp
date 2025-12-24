#include "FalconStoreCommands.h"
#include "CommandHandler.h"
#include "IStorageAPI.h"
#include "DatabaseManager.h"
#include "../../DaemonCore.h"
#include "../../../plugins/falconstore/include/FalconStore.h"
#include "Logger.h"
#include <sstream>
#include <json/json.h>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <ctime>

namespace SentinelFS {

Json::Value FalconStoreCommands::executeQuery(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        if (!data.isMember("query") || !data["query"].isString()) {
            response["success"] = false;
            response["error"] = "Query is required";
            return response;
        }
        
        std::string query = data["query"].asString();
        logger.debug("Executing query: " + query, "FalconStoreCommands");
        
        // Get DatabaseManager from daemon
        auto* dbManager = ctx_.daemonCore ? ctx_.daemonCore->getDatabase() : nullptr;
        if (!dbManager) {
            response["success"] = false;
            response["error"] = "Database not available";
            return response;
        }
        
        // Execute query and measure time
        auto start = std::chrono::high_resolution_clock::now();
        
        // Check if it's a SELECT query
        if (query.find("SELECT") == 0 || query.find("select") == 0) {
            auto result = dbManager->query(query);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            Json::Value payload(Json::objectValue);
            payload["columns"] = Json::Value(Json::arrayValue);
            payload["rows"] = Json::Value(Json::arrayValue);
            payload["executionTime"] = static_cast<double>(duration.count()) / 1000.0;
            
            if (!result.empty()) {
                // Get columns from first row
                for (const auto& [col, _] : result[0]) {
                    payload["columns"].append(col);
                }
                
                // Add all rows
                for (const auto& row : result) {
                    Json::Value jsonRow(Json::arrayValue);
                    for (const auto& [col, _] : result[0]) {
                        jsonRow.append(row.at(col));
                    }
                    payload["rows"].append(jsonRow);
                }
            }
            
            response["success"] = true;
            response["type"] = "FALCONSTORE_QUERY_RESULT";
            response["payload"] = payload;
        } else {
            // Non-SELECT query
            bool success = dbManager->execute(query);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            Json::Value payload(Json::objectValue);
            payload["affectedRows"] = success ? 1 : 0;  // Simplified - actual affected rows would need more work
            payload["executionTime"] = static_cast<double>(duration.count()) / 1000.0;
            payload["columns"] = Json::Value(Json::arrayValue);
            payload["rows"] = Json::Value(Json::arrayValue);
            
            response["success"] = success;
            response["type"] = "FALCONSTORE_QUERY_RESULT";
            response["payload"] = payload;
        }
        
    } catch (const std::exception& e) {
        logger.error("Query execution failed: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

Json::Value FalconStoreCommands::getTables(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        auto* dbManager = ctx_.daemonCore ? ctx_.daemonCore->getDatabase() : nullptr;
        if (!dbManager) {
            response["success"] = false;
            response["error"] = "Database not available";
            return response;
        }
        
        // Query table information
        auto result = dbManager->query(
            "SELECT name, sql FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name"
        );
        
        Json::Value payload(Json::arrayValue);
        
        for (const auto& row : result) {
            Json::Value table(Json::objectValue);
            table["name"] = row.at("name");
            
            // Get row count
            auto countResult = dbManager->query("SELECT COUNT(*) as count FROM " + row.at("name"));
            if (!countResult.empty()) {
                table["rowCount"] = std::stoi(countResult[0].at("count"));
            } else {
                table["rowCount"] = 0;
            }
            
            // Estimate size (simplified)
            table["size"] = 0; // Would need more complex calculation
            
            payload.append(table);
        }
        
        response["success"] = true;
        response["type"] = "FALCONSTORE_TABLES";
        response["payload"] = payload;
        
    } catch (const std::exception& e) {
        logger.error("Failed to get tables: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

Json::Value FalconStoreCommands::getTableData(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        if (!data.isMember("table") || !data["table"].isString()) {
            response["success"] = false;
            response["error"] = "Table name is required";
            return response;
        }
        
        std::string table = data["table"].asString();
        
        auto* dbManager = ctx_.daemonCore ? ctx_.daemonCore->getDatabase() : nullptr;
        if (!dbManager) {
            response["success"] = false;
            response["error"] = "Database not available";
            return response;
        }
        
        // Get table schema first
        auto schemaResult = dbManager->query("PRAGMA table_info(" + table + ")");
        
        Json::Value payload(Json::objectValue);
        payload["columns"] = Json::Value(Json::arrayValue);
        payload["rows"] = Json::Value(Json::arrayValue);
        
        // Extract column names
        for (const auto& row : schemaResult) {
            payload["columns"].append(row.at("name"));
        }
        
        // Get table data (limit to 100 rows for performance)
        auto dataResult = dbManager->query("SELECT * FROM " + table + " LIMIT 100");
        
        for (const auto& row : dataResult) {
            Json::Value jsonRow(Json::arrayValue);
            for (const auto& [col, _] : dataResult[0]) {
                jsonRow.append(row.at(col));
            }
            payload["rows"].append(jsonRow);
        }
        
        response["success"] = true;
        response["type"] = "FALCONSTORE_TABLE_DATA";
        response["payload"] = payload;
        
    } catch (const std::exception& e) {
        logger.error("Failed to get table data: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

Json::Value FalconStoreCommands::vacuum(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        auto* dbManager = ctx_.daemonCore ? ctx_.daemonCore->getDatabase() : nullptr;
        if (!dbManager) {
            response["success"] = false;
            response["error"] = "Database not available";
            return response;
        }
        
        logger.info("Starting database VACUUM...", "FalconStoreCommands");
        
        // Execute VACUUM
        dbManager->execute("VACUUM");
        
        logger.info("Database VACUUM completed", "FalconStoreCommands");
        
        response["success"] = true;
        response["message"] = "Database vacuum completed successfully";
        
    } catch (const std::exception& e) {
        logger.error("VACUUM failed: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

Json::Value FalconStoreCommands::clearCache(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        auto* storage = ctx_.storage;
        if (!storage) {
            response["success"] = false;
            response["error"] = "Storage plugin not available";
            return response;
        }
        
        // Check if storage supports DatabaseManager
        auto* dbManager = storage->getDatabaseManager();
        if (!dbManager) {
            response["success"] = false;
            response["error"] = "Storage plugin does not support cache operations";
            return response;
        }
        
        // Clear prepared statement cache
        dbManager->clearCache();
        
        logger.info("Database cache cleared", "FalconStoreCommands");
        
        response["success"] = true;
        response["message"] = "Cache cleared successfully";
        
    } catch (const std::exception& e) {
        logger.error("Failed to clear cache: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

Json::Value FalconStoreCommands::getStatus(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        auto* storage = ctx_.storage;
        if (!storage) {
            response["success"] = false;
            response["error"] = "Storage plugin not available";
            return response;
        }
        
        // Check if this is FalconStore
        if (storage->getName() != "FalconStore") {
            response["success"] = false;
            response["error"] = "FalconStore plugin not active";
            return response;
        }
        
        // Cast to FalconStore to access its methods directly
        auto* falconStore = dynamic_cast<FalconStore*>(storage);
        if (!falconStore) {
            response["success"] = false;
            response["error"] = "Not a FalconStore instance";
            return response;
        }
        
        // Get database path from environment or default
        const char* envPath = std::getenv("SENTINEL_DB_PATH");
        const char* home = std::getenv("HOME");
        std::string dbPath;
        if (envPath) {
            dbPath = envPath;
        } else if (home) {
            dbPath = std::string(home) + "/.local/share/sentinelfs/sentinel.db";
        } else {
            dbPath = "/tmp/sentinel.db";
        }
        
        // Get database size
        uint64_t dbSize = 0;
        try {
            dbSize = std::filesystem::file_size(dbPath);
        } catch (...) {
            // File might not exist
        }
        
        // Get schema version from migration manager
        int schemaVersion = 1;
        int latestVersion = 1;
        auto* migrationManager = falconStore->getMigrationManager();
        if (migrationManager) {
            schemaVersion = migrationManager->getCurrentVersion();
            latestVersion = migrationManager->getLatestVersion();
        }
        
        Json::Value payload(Json::objectValue);
        payload["plugin"] = "FalconStore";
        payload["version"] = "1.0.0";
        payload["initialized"] = true;
        payload["schemaVersion"] = schemaVersion;
        payload["latestVersion"] = latestVersion;
        payload["status"] = "running";
        payload["dbPath"] = dbPath;
        payload["dbSize"] = static_cast<Json::UInt64>(dbSize);
        
        // Cache info from FalconStore
        auto* cache = falconStore->getCache();
        if (cache) {
            auto cacheStats = cache->getStats();
            Json::Value cacheJson(Json::objectValue);
            cacheJson["enabled"] = true;
            cacheJson["entries"] = static_cast<Json::UInt64>(cacheStats.entries);
            cacheJson["hits"] = static_cast<Json::UInt64>(cacheStats.hits);
            cacheJson["misses"] = static_cast<Json::UInt64>(cacheStats.misses);
            cacheJson["hitRate"] = cacheStats.hitRate();
            cacheJson["memoryUsed"] = static_cast<Json::UInt64>(cacheStats.memoryUsed);
            payload["cache"] = cacheJson;
        } else {
            Json::Value cacheJson(Json::objectValue);
            cacheJson["enabled"] = false;
            payload["cache"] = cacheJson;
        }
        
        response["success"] = true;
        response["type"] = "FALCONSTORE_STATUS";
        response["payload"] = payload;
        
    } catch (const std::exception& e) {
        logger.error("Failed to get status: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

Json::Value FalconStoreCommands::getStats(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        auto* storage = ctx_.storage;
        if (!storage) {
            response["success"] = false;
            response["error"] = "Storage plugin not available";
            return response;
        }
        
        // Check if this is FalconStore
        if (storage->getName() != "FalconStore") {
            response["success"] = false;
            response["error"] = "FalconStore plugin not active";
            return response;
        }
        
        // Cast to FalconStore to get actual statistics
        auto* falconStore = dynamic_cast<FalconStore*>(storage);
        if (!falconStore) {
            response["success"] = false;
            response["error"] = "Not a FalconStore instance";
            return response;
        }
        
        // Get actual statistics from FalconStore
        auto stats = falconStore->getStats();
        
        Json::Value payload(Json::objectValue);
        payload["totalQueries"] = static_cast<Json::UInt64>(stats.totalQueries);
        payload["selectQueries"] = static_cast<Json::UInt64>(stats.selectQueries);
        payload["insertQueries"] = static_cast<Json::UInt64>(stats.insertQueries);
        payload["updateQueries"] = static_cast<Json::UInt64>(stats.updateQueries);
        payload["deleteQueries"] = static_cast<Json::UInt64>(stats.deleteQueries);
        payload["avgQueryTimeMs"] = stats.avgQueryTimeMs;
        payload["maxQueryTimeMs"] = stats.maxQueryTimeMs;
        payload["slowQueries"] = static_cast<Json::UInt64>(stats.slowQueries);
        payload["dbSizeBytes"] = static_cast<Json::UInt64>(stats.dbSizeBytes);
        payload["schemaVersion"] = stats.schemaVersion;
        
        // Cache statistics
        Json::Value cache(Json::objectValue);
        cache["hits"] = static_cast<Json::UInt64>(stats.cache.hits);
        cache["misses"] = static_cast<Json::UInt64>(stats.cache.misses);
        cache["entries"] = static_cast<Json::UInt64>(stats.cache.entries);
        cache["memoryUsed"] = static_cast<Json::UInt64>(stats.cache.memoryUsed);
        cache["hitRate"] = stats.cache.hitRate() * 100;  // Convert to percentage
        payload["cache"] = cache;
        
        response["success"] = true;
        response["type"] = "FALCONSTORE_STATS";
        response["payload"] = payload;
        
    } catch (const std::exception& e) {
        logger.error("Failed to get stats: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

Json::Value FalconStoreCommands::optimize(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        auto* storage = ctx_.storage;
        if (!storage) {
            response["success"] = false;
            response["error"] = "Storage not available";
            return response;
        }
        
        // Check if this is FalconStore
        if (storage->getName() != "FalconStore") {
            response["success"] = false;
            response["error"] = "FalconStore plugin not active";
            return response;
        }
        
        // Cast to FalconStore to use its optimize method
        auto* falconStore = dynamic_cast<FalconStore*>(storage);
        if (!falconStore) {
            response["success"] = false;
            response["error"] = "Not a FalconStore instance";
            return response;
        }
        
        logger.info("Starting database optimization...", "FalconStoreCommands");
        
        // Use FalconStore's optimize method which handles VACUUM properly
        falconStore->optimize();
        
        logger.info("Database optimization completed", "FalconStoreCommands");
        
        response["success"] = true;
        response["message"] = "Database optimization completed successfully";
        
    } catch (const std::exception& e) {
        logger.error("Optimization failed: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

Json::Value FalconStoreCommands::backup(const std::string& args, const Json::Value& data) {
    Json::Value response(Json::objectValue);
    auto& logger = Logger::instance();
    
    try {
        auto* dbManager = ctx_.daemonCore ? ctx_.daemonCore->getDatabase() : nullptr;
        if (!dbManager) {
            response["success"] = false;
            response["error"] = "Database not available";
            return response;
        }
        
        // Generate backup path with timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << "/tmp/sentinel_backup_" 
            << std::put_time(&tm, "%Y%m%d_%H%M%S") 
            << ".db";
        std::string backupPath = oss.str();
        
        logger.info("Creating database backup: " + backupPath, "FalconStoreCommands");
        
        // Execute backup via VACUUM INTO
        std::string backupSql = "VACUUM INTO '" + backupPath + "'";
        dbManager->execute(backupSql);
        
        logger.info("Database backup created: " + backupPath, "FalconStoreCommands");
        
        response["success"] = true;
        response["message"] = "Backup created successfully";
        response["backupPath"] = backupPath;
        
    } catch (const std::exception& e) {
        logger.error("Backup failed: " + std::string(e.what()), "FalconStoreCommands");
        response["success"] = false;
        response["error"] = e.what();
    }
    
    return response;
}

} // namespace SentinelFS
