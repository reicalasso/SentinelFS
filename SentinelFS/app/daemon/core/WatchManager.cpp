/**
 * @file WatchManager.cpp
 * @brief DaemonCore watch directory and event handler management
 */

#include "DaemonCore.h"
#include "Logger.h"
#include <filesystem>
#include <ctime>
#include <sqlite3.h>

namespace SentinelFS {

void DaemonCore::setupEventHandlers() {
    // Event handlers are now in EventHandlers class
    // This method is kept for internal initialization if needed
}

bool DaemonCore::addWatchDirectory(const std::string& path) {
    auto& logger = Logger::instance();
    
    // Validate path
    if (!std::filesystem::exists(path)) {
        logger.error("Directory does not exist: " + path, "DaemonCore");
        return false;
    }
    
    if (!std::filesystem::is_directory(path)) {
        logger.error("Path is not a directory: " + path, "DaemonCore");
        return false;
    }
    
    // Add watch via filesystem plugin
    if (!filesystem_) {
        logger.error("Filesystem plugin not initialized", "DaemonCore");
        return false;
    }
    
    try {
        // Get absolute path
        auto absPath = std::filesystem::absolute(path).string();
        
        // Add watch (filesystem plugin should support multiple watches)
        logger.info("Adding watch for directory: " + absPath, "DaemonCore");
        
        // Save to database using API (tracks statistics properly)
        if (storage_) {
            bool exists = storage_->isWatchedFolder(absPath);
            
            if (exists) {
                // Update existing folder to active
                storage_->updateWatchedFolderStatus(absPath, 1);
                logger.info("Reactivated watched folder: " + absPath, "DaemonCore");
            } else {
                // Insert new folder
                if (storage_->addWatchedFolder(absPath)) {
                    logger.info("Watched folder saved to database: " + absPath, "DaemonCore");
                } else {
                    logger.error("Failed to save watched folder to database", "DaemonCore");
                }
            }
        }
        
        // Start watching the directory with filesystem plugin
        filesystem_->startWatching(absPath);
        logger.info("Directory watch added successfully: " + absPath, "DaemonCore");
        
        // Trigger immediate scan of the new directory
        eventBus_.publish("WATCH_ADDED", absPath);
        
        return true;
        
    } catch (const std::exception& e) {
        logger.error("Failed to add watch: " + std::string(e.what()), "DaemonCore");
        return false;
    }
}

} // namespace SentinelFS
