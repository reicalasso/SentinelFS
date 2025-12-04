#pragma once

#include "CommandHandler.h"
#include <string>

namespace SentinelFS {

/**
 * @brief Handles configuration related IPC commands
 * 
 * Commands: CONFIG_JSON, SET_CONFIG, EXPORT_CONFIG, IMPORT_CONFIG,
 *           ADD_IGNORE, REMOVE_IGNORE, LIST_IGNORE, EXPORT_SUPPORT_BUNDLE
 */
class ConfigCommands : public CommandHandler {
public:
    explicit ConfigCommands(CommandContext& ctx) : CommandHandler(ctx) {}
    
    // JSON commands for GUI
    std::string handleConfigJson();
    std::string handleSetConfig(const std::string& args);
    std::string handleExportConfig();
    std::string handleImportConfig(const std::string& args);
    
    // Ignore patterns
    std::string handleAddIgnore(const std::string& args);
    std::string handleRemoveIgnore(const std::string& args);
    std::string handleListIgnore();
    
    // Support bundle
    std::string handleExportSupportBundle();
};

} // namespace SentinelFS
