#include "FileSyncHandler.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <regex>
#include <sqlite3.h>
#include <fnmatch.h>

namespace SentinelFS {

FileSyncHandler::FileSyncHandler(INetworkAPI* network, IStorageAPI* storage, const std::string& watchDir)
    : network_(network), storage_(storage), watchDirectory_(watchDir) {
    auto& logger = Logger::instance();
    logger.debug("FileSyncHandler initialized for: " + watchDir, "FileSyncHandler");
    loadIgnorePatterns();
}

void FileSyncHandler::loadIgnorePatterns() {
    ignorePatterns_.clear();
    if (!storage_) return;
    
    sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
    if (!db) return;
    
    const char* sql = "SELECT pattern FROM ignore_patterns WHERE active = 1";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* pattern = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (pattern) {
                ignorePatterns_.push_back(pattern);
            }
        }
        sqlite3_finalize(stmt);
    }
    
    auto& logger = Logger::instance();
    logger.debug("Loaded " + std::to_string(ignorePatterns_.size()) + " ignore patterns", "FileSyncHandler");
}

bool FileSyncHandler::shouldIgnore(const std::string& absolutePath) {
    // Calculate relative path
    std::string relativePath = absolutePath;
    if (absolutePath.find(watchDirectory_) == 0) {
        relativePath = absolutePath.substr(watchDirectory_.length());
        if (!relativePath.empty() && relativePath[0] == '/') {
            relativePath = relativePath.substr(1);
        }
    }
    
    std::string filename = std::filesystem::path(absolutePath).filename().string();
    
    for (const auto& pattern : ignorePatterns_) {
        // 1. Check exact filename match (e.g. "*.log")
        // Using 0 instead of FNM_PATHNAME allows * to match across directories for simple globs
        if (fnmatch(pattern.c_str(), filename.c_str(), 0) == 0) {
            return true;
        }
        
        // 2. Check relative path match (e.g. "src/temp/*")
        if (fnmatch(pattern.c_str(), relativePath.c_str(), 0) == 0) {
            return true;
        }
        
        // 3. Check directory patterns (ending with /)
        if (pattern.back() == '/') {
            std::string dirPattern = pattern.substr(0, pattern.length() - 1);
            
            // Check if this IS the ignored directory
            if (filename == dirPattern) return true;
            
            // Check if relative path starts with this directory
            if (relativePath.find(pattern) == 0) return true;
            
            // Check if it's a component in the path (e.g. "src/node_modules/foo")
            // We want to match "/node_modules/" inside the path
            std::string component = "/" + dirPattern + "/";
            if (("/" + relativePath).find(component) != std::string::npos) return true;
        }
    }
    return false;
}

std::string FileSyncHandler::calculateFileHash(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        EVP_DigestUpdate(ctx, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        EVP_DigestUpdate(ctx, buffer, file.gcount());
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(ctx, hash, &hashLen);
    EVP_MD_CTX_free(ctx);

    std::stringstream ss;
    for(unsigned int i = 0; i < hashLen; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

void FileSyncHandler::scanDirectory(const std::string& path) {
    auto& logger = Logger::instance();
    std::string targetPath = path.empty() ? watchDirectory_ : path;
    
    logger.info("Starting scan of directory: " + targetPath, "FileSyncHandler");

    if (!std::filesystem::exists(targetPath)) {
        logger.warn("Directory does not exist, skipping scan: " + targetPath, "FileSyncHandler");
        return;
    }

    // Reload ignore patterns before scanning
    loadIgnorePatterns();

    try {
        int count = 0;
        int ignored = 0;
        
        // Use iterator directly to control recursion
        for (auto it = std::filesystem::recursive_directory_iterator(targetPath); 
             it != std::filesystem::recursive_directory_iterator(); 
             ++it) {
            
            std::string currentPath = it->path().string();
            
            // Check ignore BEFORE processing file or entering directory
            if (shouldIgnore(currentPath)) {
                ignored++;
                if (it->is_directory()) {
                    // Don't scan inside ignored directories (like node_modules)
                    it.disable_recursion_pending();
                    logger.debug("Ignoring directory and its children: " + currentPath, "FileSyncHandler");
                }
                continue;
            }
            
            if (it->is_regular_file()) {
                std::string hash = calculateFileHash(currentPath);
                long long size = std::filesystem::file_size(it->path());
                long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    std::filesystem::last_write_time(it->path()).time_since_epoch()
                ).count();
                
                if (storage_->addFile(currentPath, hash, timestamp, size)) {
                    count++;
                }
            }
        }
        logger.info("Scan completed. Found " + std::to_string(count) + " files, ignored " + std::to_string(ignored), "FileSyncHandler");
    } catch (const std::exception& e) {
        logger.error("Error during directory scan: " + std::string(e.what()), "FileSyncHandler");
    }
}

bool FileSyncHandler::updateFileInDatabase(const std::string& fullPath) {
    auto& logger = Logger::instance();
    std::string filename = std::filesystem::path(fullPath).filename().string();
    
    try {
        // Check if file exists
        if (!std::filesystem::exists(fullPath)) {
            storage_->removeFile(fullPath);
            logger.info("File removed from DB: " + filename, "FileSyncHandler");
            return false;
        }

        // Calculate hash and metadata
        std::string hash = calculateFileHash(fullPath);
        long long size = std::filesystem::file_size(fullPath);
        long long timestamp = std::time(nullptr);

        // Update database
        // Note: addFile() uses INSERT OR IGNORE + UPDATE, which preserves synced status for existing files
        // and sets synced=0 for new files
        if (storage_->addFile(fullPath, hash, timestamp, size)) {
            logger.info("💾 Database updated for file: " + filename + " (" + std::to_string(size) + " bytes)" + 
                       (syncEnabled_ ? " [will broadcast]" : " [pending - paused]"), "FileSyncHandler");
            return true;
        } else {
            logger.error("Failed to update database for file: " + filename, "FileSyncHandler");
            return false;
        }
    } catch (const std::exception& e) {
        logger.error("Error updating database for " + filename + ": " + std::string(e.what()), "FileSyncHandler");
        return false;
    }
}

void FileSyncHandler::broadcastUpdate(const std::string& fullPath) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    std::string filename = std::filesystem::path(fullPath).filename().string();
    
    try {
        if (!std::filesystem::exists(fullPath)) {
            logger.warn("Cannot broadcast - file no longer exists: " + filename, "FileSyncHandler");
            return;
        }

        // Get file info for broadcast
        std::string hash = calculateFileHash(fullPath);
        long long size = std::filesystem::file_size(fullPath);
        
        // Get connected peers
        auto peers = storage_->getAllPeers();
        
        if (peers.empty()) {
            logger.debug("No peers connected, marking file as synced locally: " + filename, "FileSyncHandler");
            
            // Mark as synced even without peers (no one to broadcast to)
            sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
            const char* updateSyncedSql = "UPDATE files SET synced = 1 WHERE path = ?";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, updateSyncedSql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, fullPath.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
            return;
        }
        
        logger.info("📡 Broadcasting update for " + filename + " to " + std::to_string(peers.size()) + " peer(s)", "FileSyncHandler");
        
        // Calculate relative path
        std::string relativePath = fullPath;
        if (fullPath.find(watchDirectory_) == 0) {
            relativePath = fullPath.substr(watchDirectory_.length());
            if (!relativePath.empty() && relativePath[0] == '/') {
                relativePath = relativePath.substr(1);
            }
        }
        
        // Broadcast UPDATE_AVAILABLE with relative path
        std::string updateMsg = "UPDATE_AVAILABLE|" + relativePath + "|" + hash + "|" + std::to_string(size);
        std::vector<uint8_t> payload(updateMsg.begin(), updateMsg.end());
        
        int successCount = 0;
        int failCount = 0;
        
        for (const auto& peer : peers) {
            try {
                bool sent = network_->sendData(peer.id, payload);
                if (sent) {
                    successCount++;
                    logger.debug("Sent update notification to peer: " + peer.id, "FileSyncHandler");
                } else {
                    failCount++;
                    logger.warn("Failed to send update to peer: " + peer.id, "FileSyncHandler");
                }
            } catch (const std::exception& e) {
                failCount++;
                logger.error("Exception sending to peer " + peer.id + ": " + std::string(e.what()), "FileSyncHandler");
            }
        }
        
        if (successCount > 0) {
            metrics.incrementFilesSynced();
            
            // Mark file as synced after successful broadcast
            sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
            const char* updateSyncedSql = "UPDATE files SET synced = 1 WHERE path = ?";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, updateSyncedSql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, fullPath.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }
        
        if (failCount > 0) {
            metrics.incrementSyncErrors();
            logger.warn("Update broadcast completed with " + std::to_string(failCount) + " failure(s)", "FileSyncHandler");
        } else {
            logger.debug("Update broadcast successful to all peers", "FileSyncHandler");
        }
        
    } catch (const std::exception& e) {
        logger.error("Error broadcasting update for " + filename + ": " + std::string(e.what()), "FileSyncHandler");
        metrics.incrementSyncErrors();
    }
}

void FileSyncHandler::handleFileModified(const std::string& fullPath) {
    auto& logger = Logger::instance();
    std::string filename = std::filesystem::path(fullPath).filename().string();
    
    // Check ignore patterns
    if (shouldIgnore(fullPath)) {
        logger.debug("File ignored by pattern: " + filename, "FileSyncHandler");
        return;
    }
    
    // ALWAYS update database (even when sync is paused)
    // This ensures GUI shows correct file information
    bool dbUpdated = updateFileInDatabase(fullPath);
    
    if (!dbUpdated) {
        logger.warn("Skipping broadcast - database update failed for: " + filename, "FileSyncHandler");
        return;
    }
    
    // Only broadcast if sync is enabled
    if (!syncEnabled_) {
        logger.info("⏸️  Sync paused - database updated but broadcast skipped for: " + filename, "FileSyncHandler");
        return;
    }
    
    // Broadcast to peers
    broadcastUpdate(fullPath);
}

} // namespace SentinelFS
