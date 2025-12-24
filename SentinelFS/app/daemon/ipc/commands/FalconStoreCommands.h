#pragma once

#include "CommandHandler.h"
#include <vector>
#include <string>
#include <json/json.h>

namespace SentinelFS {

struct CommandContext;

/**
 * @brief IPC commands for FalconStore enhanced GUI features
 */
class FalconStoreCommands {
public:
    explicit FalconStoreCommands(const CommandContext& ctx) : ctx_(ctx) {}
    
    /**
     * @brief Execute a raw SQL query
     */
    Json::Value executeQuery(const std::string& args, const Json::Value& data);
    
    /**
     * @brief Get list of all database tables
     */
    Json::Value getTables(const std::string& args, const Json::Value& data);
    
    /**
     * @brief Get data from a specific table
     */
    Json::Value getTableData(const std::string& args, const Json::Value& data);
    
    /**
     * @brief Vacuum the database
     */
    Json::Value vacuum(const std::string& args, const Json::Value& data);
    
    /**
     * @brief Clear database cache
     */
    Json::Value clearCache(const std::string& args, const Json::Value& data);
    
    /**
     * @brief Get FalconStore status information
     */
    Json::Value getStatus(const std::string& args, const Json::Value& data);
    
    /**
     * @brief Get FalconStore statistics
     */
    Json::Value getStats(const std::string& args, const Json::Value& data);
    
    /**
     * @brief Optimize database (VACUUM + ANALYZE)
     */
    Json::Value optimize(const std::string& args, const Json::Value& data);
    
    /**
     * @brief Create database backup
     */
    Json::Value backup(const std::string& args, const Json::Value& data);
    
private:
    const CommandContext& ctx_;
};

} // namespace SentinelFS
