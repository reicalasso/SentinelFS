#pragma once

#include "CommandHandler.h"
#include <string>

namespace SentinelFS {

/**
 * @brief Handles file and folder related IPC commands
 * 
 * Commands: FILES_JSON, ADD_FOLDER, REMOVE_WATCH, ACTIVITY_JSON,
 *           CONFLICTS_JSON, RESOLVE_CONFLICT, SYNC_QUEUE_JSON,
 *           VERSIONS_JSON, RESTORE_VERSION, DELETE_VERSION, PREVIEW_VERSION
 */
class FileCommands : public CommandHandler {
public:
    explicit FileCommands(CommandContext& ctx) : CommandHandler(ctx) {}
    
    // File listing
    std::string handleFilesJson();
    std::string handleActivityJson();
    
    // Folder management
    std::string handleAddFolder(const std::string& args);
    std::string handleRemoveWatch(const std::string& args);
    
    // Conflicts
    std::string handleConflicts();
    std::string handleConflictsJson();
    std::string handleResolve(const std::string& args);
    std::string handleResolveConflict(const std::string& args);
    
    // Sync queue
    std::string handleSyncQueueJson();
    
    // Version management
    std::string handleVersionsJson();
    std::string handleRestoreVersion(const std::string& args);
    std::string handleDeleteVersion(const std::string& args);
    std::string handlePreviewVersion(const std::string& args);
    
private:
    std::string sanitizePath(const std::string& path);
};

} // namespace SentinelFS
