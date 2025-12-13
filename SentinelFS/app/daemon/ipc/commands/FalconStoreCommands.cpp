#include "FalconStoreCommands.h"
#include "CommandHandler.h"
#include "IStorageAPI.h"
#include "DatabaseManager.h"
#include "../../DaemonCore.h"
#include "Logger.h"
#include <sstream>
#include <json/json.h>

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

} // namespace SentinelFS
