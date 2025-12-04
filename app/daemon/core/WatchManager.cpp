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
        
        // Save to database (or reactivate if exists)
        if (storage_) {
            sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
            
            // First check if folder already exists
            const char* checkSql = "SELECT status FROM watched_folders WHERE path = ?";
            sqlite3_stmt* checkStmt;
            bool exists = false;
            
            if (sqlite3_prepare_v2(db, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(checkStmt, 1, absPath.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(checkStmt) == SQLITE_ROW) {
                    exists = true;
                }
                sqlite3_finalize(checkStmt);
            }
            
            if (exists) {
                // Update existing folder to active
                const char* updateSql = "UPDATE watched_folders SET status = 'active', added_at = ? WHERE path = ?";
                sqlite3_stmt* updateStmt;
                
                if (sqlite3_prepare_v2(db, updateSql, -1, &updateStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int64(updateStmt, 1, std::time(nullptr));
                    sqlite3_bind_text(updateStmt, 2, absPath.c_str(), -1, SQLITE_TRANSIENT);
                    
                    if (sqlite3_step(updateStmt) == SQLITE_DONE) {
                        logger.info("Reactivated watched folder: " + absPath, "DaemonCore");
                    }
                    sqlite3_finalize(updateStmt);
                }
            } else {
                // Insert new folder
                const char* insertSql = "INSERT INTO watched_folders (path, added_at, status) VALUES (?, ?, 'active')";
                sqlite3_stmt* insertStmt;
                
                if (sqlite3_prepare_v2(db, insertSql, -1, &insertStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(insertStmt, 1, absPath.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int64(insertStmt, 2, std::time(nullptr));
                    
                    if (sqlite3_step(insertStmt) == SQLITE_DONE) {
                        logger.info("Watched folder saved to database: " + absPath, "DaemonCore");
                    } else {
                        logger.error("Failed to save watched folder to database", "DaemonCore");
                    }
                    sqlite3_finalize(insertStmt);
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
