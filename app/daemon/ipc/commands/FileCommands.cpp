#include "FileCommands.h"
#include "../../DaemonCore.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include "MetricsCollector.h"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sqlite3.h>
#include <ctime>

namespace SentinelFS {

std::string FileCommands::sanitizePath(const std::string& path) {
    std::string cleanPath = path;
    
    // Remove file:// prefix
    if (cleanPath.find("file://") == 0) {
        cleanPath = cleanPath.substr(7);
    }
    
    // Replace unicode fraction slash (⁄ U+2044) with regular slash
    size_t pos = 0;
    while ((pos = cleanPath.find("\xe2\x81\x84", pos)) != std::string::npos) {
        cleanPath.replace(pos, 3, "/");
    }
    
    return cleanPath;
}

std::string FileCommands::handleFilesJson() {
    std::stringstream ss;
    ss << "{\"files\": [";
    
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        bool first = true;
        std::vector<std::string> watchedFolders;
        
        // 1. Get watched folders first (Roots)
        const char* folderSql = "SELECT path FROM watched_folders WHERE status = 'active'";
        sqlite3_stmt* folderStmt;
        
        if (sqlite3_prepare_v2(db, folderSql, -1, &folderStmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(folderStmt) == SQLITE_ROW) {
                const char* folderPath = reinterpret_cast<const char*>(sqlite3_column_text(folderStmt, 0));
                if (folderPath) {
                    std::string pathStr = folderPath;
                    watchedFolders.push_back(pathStr);
                    
                    // Calculate folder size from files table
                    int64_t folderSize = 0;
                    std::string sizeSql = "SELECT COALESCE(SUM(size), 0) FROM files WHERE path LIKE ?";
                    sqlite3_stmt* sizeStmt;
                    if (sqlite3_prepare_v2(db, sizeSql.c_str(), -1, &sizeStmt, nullptr) == SQLITE_OK) {
                        std::string pattern = pathStr + "/%";
                        sqlite3_bind_text(sizeStmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(sizeStmt) == SQLITE_ROW) {
                            folderSize = sqlite3_column_int64(sizeStmt, 0);
                        }
                        sqlite3_finalize(sizeStmt);
                    }
                    
                    if (!first) ss << ",";
                    first = false;
                    
                    ss << "{";
                    ss << "\"path\":\"" << pathStr << "\",";
                    ss << "\"hash\":\"\",";
                    ss << "\"size\":" << folderSize << ",";
                    ss << "\"lastModified\":" << std::time(nullptr) << ",";
                    ss << "\"syncStatus\":\"watching\",";
                    ss << "\"isFolder\":true";
                    ss << "}";
                }
            }
            sqlite3_finalize(folderStmt);
        }

        // 2. Get all files from the files table
        const char* sql = "SELECT path, hash, timestamp, size, synced FROM files ORDER BY timestamp DESC LIMIT 1000";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (!path) continue;
                
                std::string pathStr = path;
                
                // Determine parent folder
                std::string parent = "";
                size_t maxLen = 0;
                
                for (const auto& folder : watchedFolders) {
                    if (pathStr.find(folder) == 0) {
                        if (pathStr.length() == folder.length() || pathStr[folder.length()] == std::filesystem::path::preferred_separator) {
                             if (folder.length() > maxLen) {
                                 maxLen = folder.length();
                                 parent = folder;
                             }
                        }
                    }
                }
                
                // Skip files that don't belong to any active watched folder
                if (parent.empty()) {
                    continue;
                }
                
                if (!first) ss << ",";
                first = false;
                
                const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                int64_t timestamp = sqlite3_column_int64(stmt, 2);
                int64_t size = sqlite3_column_int64(stmt, 3);
                int synced = sqlite3_column_int(stmt, 4);
                
                ss << "{";
                ss << "\"path\":\"" << pathStr << "\",";
                ss << "\"hash\":\"" << (hash ? hash : "") << "\",";
                ss << "\"size\":" << size << ",";
                ss << "\"lastModified\":" << timestamp << ",";
                ss << "\"syncStatus\":\"" << (synced ? "synced" : "pending") << "\"";
                if (!parent.empty()) {
                    ss << ",\"parent\":\"" << parent << "\"";
                }
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string FileCommands::handleActivityJson() {
    std::stringstream ss;
    ss << "{\"activity\": [";
    bool first = true;
    
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        
        // Get recently synced files
        const char* filesSql = "SELECT path, timestamp, synced FROM files ORDER BY timestamp DESC LIMIT 10";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, filesSql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                long long timestamp = sqlite3_column_int64(stmt, 1);
                int synced = sqlite3_column_int(stmt, 2);
                
                std::string pathStr = path ? path : "";
                std::filesystem::path p(pathStr);
                std::string filename = p.filename().string();
                if (filename.empty()) filename = pathStr;
                
                std::string timeStr = formatTime(timestamp);
                
                ss << "{";
                ss << "\"type\":\"" << (synced ? "sync" : "modified") << "\",";
                ss << "\"file\":\"" << filename << "\",";
                ss << "\"time\":\"" << timeStr << "\",";
                ss << "\"details\":\"" << (synced ? "File synced" : "File modified") << "\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
        
        // Also include watched folders
        const char* foldersSql = "SELECT path, added_at FROM watched_folders WHERE status = 'active' ORDER BY added_at DESC LIMIT 3";
        
        if (sqlite3_prepare_v2(db, foldersSql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                long long addedAt = sqlite3_column_int64(stmt, 1);
                
                std::string pathStr = path ? path : "";
                std::filesystem::path p(pathStr);
                std::string filename = p.filename().string();
                if (filename.empty()) filename = pathStr;
                
                std::string timeStr = formatTime(addedAt);
                
                ss << "{";
                ss << "\"type\":\"folder\",";
                ss << "\"file\":\"" << filename << "\",";
                ss << "\"time\":\"" << timeStr << "\",";
                ss << "\"details\":\"Folder watching started\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string FileCommands::handleAddFolder(const std::string& args) {
    if (args.empty()) {
        return "Error: No folder path provided\n";
    }
    
    if (!ctx_.daemonCore) {
        return "Error: Daemon core not initialized\n";
    }
    
    std::string cleanPath = sanitizePath(args);
    
    if (ctx_.daemonCore->addWatchDirectory(cleanPath)) {
        return "Success: Folder added to watch list: " + cleanPath + "\n";
    } else {
        return "Error: Failed to add folder to watch list: " + cleanPath + "\n";
    }
}

std::string FileCommands::handleRemoveWatch(const std::string& args) {
    if (args.empty()) {
        return "Error: No path provided\n";
    }
    
    if (!ctx_.storage) {
        return "Error: Storage not initialized\n";
    }
    
    std::string cleanPath = sanitizePath(args);
    
    sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
    sqlite3_stmt* stmt;
    
    // Count files that will be removed from monitoring
    std::string folderPrefix = cleanPath;
    if (!folderPrefix.empty() && folderPrefix.back() != '/') {
        folderPrefix += '/';
    }
    
    int fileCount = 0;
    const char* countSql = "SELECT COUNT(*) FROM files WHERE path LIKE ?";
    if (sqlite3_prepare_v2(db, countSql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string pattern = folderPrefix + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fileCount = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // Remove files from database (but NOT from disk)
    const char* deleteFilesSql = "DELETE FROM files WHERE path LIKE ?";
    if (sqlite3_prepare_v2(db, deleteFilesSql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string pattern = folderPrefix + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Remove watched folder from database
    const char* deleteFolderSql = "DELETE FROM watched_folders WHERE path = ?";
    if (sqlite3_prepare_v2(db, deleteFolderSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, cleanPath.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            
            // Stop filesystem watcher
            if (ctx_.filesystem) {
                ctx_.filesystem->stopWatching(cleanPath);
            }
            
            // Reset threat metrics since watched files changed
            MetricsCollector::instance().resetThreatMetrics();
            
            return "Success: Stopped watching " + cleanPath + " (" + 
                   std::to_string(fileCount) + " files remain on disk and will no longer be monitored)\n";
        }
        sqlite3_finalize(stmt);
    }
    
    return "Error: Failed to remove watch for: " + cleanPath + "\n";
}

std::string FileCommands::handleConflicts() {
    auto conflicts = ctx_.storage->getUnresolvedConflicts();
    std::stringstream ss;
    ss << "=== File Conflicts ===\n";
    
    if (conflicts.empty()) {
        ss << "No conflicts detected. ✓\n";
    } else {
        ss << "Found " << conflicts.size() << " unresolved conflict(s):\n\n";
        for (const auto& c : conflicts) {
            ss << "  ID: " << c.id << "\n";
            ss << "  File: " << c.path << "\n";
            ss << "  Remote Peer: " << c.remotePeerId << "\n";
            ss << "  Local: " << c.localSize << " bytes @ " << c.localTimestamp << "\n";
            ss << "  Remote: " << c.remoteSize << " bytes @ " << c.remoteTimestamp << "\n";
            ss << "  Strategy: " << c.strategy << "\n";
            ss << "  ---\n";
        }
    }
    
    auto [total, unresolved] = ctx_.storage->getConflictStats();
    ss << "\nTotal conflicts: " << total << " (Unresolved: " << unresolved << ")\n";
    return ss.str();
}

std::string FileCommands::handleConflictsJson() {
    std::stringstream ss;
    ss << "{\"conflicts\":[";
    
    if (ctx_.storage) {
        auto conflicts = ctx_.storage->getUnresolvedConflicts();
        bool first = true;
        for (const auto& c : conflicts) {
            if (!first) ss << ",";
            first = false;
            ss << "{";
            ss << "\"id\":" << c.id << ",";
            ss << "\"path\":\"" << c.path << "\",";
            ss << "\"localSize\":" << c.localSize << ",";
            ss << "\"remoteSize\":" << c.remoteSize << ",";
            ss << "\"localTimestamp\":" << c.localTimestamp << ",";
            ss << "\"remoteTimestamp\":" << c.remoteTimestamp << ",";
            ss << "\"remotePeerId\":\"" << c.remotePeerId << "\",";
            ss << "\"strategy\":" << c.strategy;
            ss << "}";
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string FileCommands::handleResolve(const std::string& args) {
    try {
        std::istringstream iss(args);
        int conflictId;
        std::string resolution;
        iss >> conflictId >> resolution;
        
        std::transform(resolution.begin(), resolution.end(), resolution.begin(), ::toupper);
        
        std::string strategy = "manual";
        if (resolution == "LOCAL") {
            strategy = "local_wins";
        } else if (resolution == "REMOTE") {
            strategy = "remote_wins";
        } else if (resolution == "BOTH") {
            strategy = "keep_both";
        }
        
        if (ctx_.storage->markConflictResolved(conflictId)) {
            return "Conflict " + std::to_string(conflictId) + " resolved with strategy: " + strategy + "\n";
        } else {
            return "Failed to resolve conflict " + std::to_string(conflictId) + ".\n";
        }
    } catch (const std::invalid_argument&) {
        return "Invalid conflict ID.\n";
    } catch (const std::out_of_range&) {
        return "Conflict ID out of range.\n";
    }
}

std::string FileCommands::handleResolveConflict(const std::string& args) {
    std::istringstream iss(args);
    int conflictId;
    std::string resolution;
    
    if (!(iss >> conflictId >> resolution)) {
        return "Error: Usage: RESOLVE_CONFLICT <id> <local|remote|both>\n";
    }
    
    if (!ctx_.storage) {
        return "Error: Storage not initialized\n";
    }
    
    sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
    const char* sql = "UPDATE conflicts SET resolved = 1, resolved_at = datetime('now'), strategy = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    int strategy = 0;
    std::string msg;
    
    if (resolution == "local") {
        strategy = 0;
        msg = "keeping local version";
    } else if (resolution == "remote") {
        strategy = 1;
        msg = "keeping remote version";
    } else if (resolution == "both") {
        strategy = 2;
        msg = "keeping both versions";
    } else {
        return "Error: Invalid resolution. Use: local, remote, or both\n";
    }
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, strategy);
        sqlite3_bind_int(stmt, 2, conflictId);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return "Success: Conflict resolved - " + msg + "\n";
        }
        sqlite3_finalize(stmt);
    }
    
    return "Error: Failed to resolve conflict\n";
}

std::string FileCommands::handleSyncQueueJson() {
    std::stringstream ss;
    ss << "{\"queue\":[";
    
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        const char* sql = "SELECT id, file_path, operation, status, progress, size, peer_id, created_at FROM sync_queue ORDER BY created_at DESC LIMIT 50";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            bool first = true;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                int id = sqlite3_column_int(stmt, 0);
                const char* path = (const char*)sqlite3_column_text(stmt, 1);
                const char* op = (const char*)sqlite3_column_text(stmt, 2);
                const char* status = (const char*)sqlite3_column_text(stmt, 3);
                int progress = sqlite3_column_int(stmt, 4);
                int64_t size = sqlite3_column_int64(stmt, 5);
                const char* peer = (const char*)sqlite3_column_text(stmt, 6);
                const char* created = (const char*)sqlite3_column_text(stmt, 7);
                
                ss << "{";
                ss << "\"id\":" << id << ",";
                ss << "\"path\":\"" << (path ? path : "") << "\",";
                ss << "\"operation\":\"" << (op ? op : "") << "\",";
                ss << "\"status\":\"" << (status ? status : "") << "\",";
                ss << "\"progress\":" << progress << ",";
                ss << "\"size\":" << size << ",";
                ss << "\"peer\":\"" << (peer ? peer : "") << "\",";
                ss << "\"created\":\"" << (created ? created : "") << "\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string FileCommands::handleVersionsJson() {
    if (!ctx_.storage) {
        return "{\"type\":\"VERSIONS\",\"payload\":{}}\n";
    }
    
    std::stringstream ss;
    ss << "{\"type\":\"VERSIONS\",\"payload\":{";
    
    std::string versionDir;
    if (ctx_.daemonCore) {
        auto config = ctx_.daemonCore->getConfig();
        versionDir = config.watchDirectory + "/.sentinel_versions";
    }
    
    namespace fs = std::filesystem;
    bool first = true;
    
    if (!versionDir.empty() && fs::exists(versionDir)) {
        for (const auto& entry : fs::directory_iterator(versionDir)) {
            if (entry.is_directory()) {
                std::string metadataPath = entry.path().string() + "/metadata.json";
                if (fs::exists(metadataPath)) {
                    std::ifstream metaFile(metadataPath);
                    std::stringstream buffer;
                    buffer << metaFile.rdbuf();
                    std::string content = buffer.str();
                    
                    size_t filePathPos = content.find("\"filePath\":\"");
                    if (filePathPos != std::string::npos) {
                        filePathPos += 12;
                        size_t endPos = content.find("\"", filePathPos);
                        if (endPos != std::string::npos) {
                            std::string filePath = content.substr(filePathPos, endPos - filePathPos);
                            
                            if (!first) ss << ",";
                            first = false;
                            
                            ss << "\"" << filePath << "\":" << content;
                        }
                    }
                }
            }
        }
    }
    
    ss << "}}\n";
    return ss.str();
}

std::string FileCommands::handleRestoreVersion(const std::string& args) {
    std::istringstream iss(args);
    int versionId;
    std::string pathOrId;
    iss >> versionId >> pathOrId;
    
    if (pathOrId.empty()) {
        return "Error: Usage: RESTORE_VERSION <versionId> <filePath>\n";
    }
    
    namespace fs = std::filesystem;
    std::string versionDir;
    
    if (ctx_.daemonCore) {
        auto config = ctx_.daemonCore->getConfig();
        versionDir = config.watchDirectory + "/.sentinel_versions";
    }
    
    if (versionDir.empty() || !fs::exists(versionDir)) {
        return "Error: Version storage not found\n";
    }
    
    for (const auto& entry : fs::directory_iterator(versionDir)) {
        if (entry.is_directory()) {
            std::string metadataPath = entry.path().string() + "/metadata.json";
            if (fs::exists(metadataPath)) {
                std::ifstream metaFile(metadataPath);
                std::stringstream buffer;
                buffer << metaFile.rdbuf();
                std::string content = buffer.str();
                
                std::string versionIdStr = "\"versionId\":" + std::to_string(versionId);
                if (content.find(versionIdStr) != std::string::npos) {
                    size_t pos = content.find(versionIdStr);
                    size_t versionPathPos = content.find("\"versionPath\":\"", pos);
                    if (versionPathPos != std::string::npos) {
                        versionPathPos += 15;
                        size_t endPos = content.find("\"", versionPathPos);
                        std::string versionPath = content.substr(versionPathPos, endPos - versionPathPos);
                        
                        size_t filePathPos = content.find("\"filePath\":\"", pos);
                        if (filePathPos != std::string::npos) {
                            filePathPos += 12;
                            size_t filePathEnd = content.find("\"", filePathPos);
                            std::string filePath = content.substr(filePathPos, filePathEnd - filePathPos);
                            
                            try {
                                if (fs::exists(versionPath)) {
                                    fs::copy_file(versionPath, filePath, fs::copy_options::overwrite_existing);
                                    return "Success: Restored version " + std::to_string(versionId) + " to " + filePath + "\n";
                                }
                            } catch (const fs::filesystem_error& e) {
                                return "Error: Failed to restore version: " + std::string(e.what()) + "\n";
                            }
                        }
                    }
                }
            }
        }
    }
    
    return "Error: Version not found\n";
}

std::string FileCommands::handleDeleteVersion(const std::string& args) {
    std::istringstream iss(args);
    int versionId;
    std::string filePath;
    iss >> versionId;
    std::getline(iss >> std::ws, filePath);
    
    if (filePath.empty()) {
        return "Error: Usage: DELETE_VERSION <versionId> <filePath>\n";
    }
    
    namespace fs = std::filesystem;
    std::string versionDir;
    
    if (ctx_.daemonCore) {
        auto config = ctx_.daemonCore->getConfig();
        versionDir = config.watchDirectory + "/.sentinel_versions";
    }
    
    if (versionDir.empty() || !fs::exists(versionDir)) {
        return "Error: Version storage not found\n";
    }
    
    for (const auto& entry : fs::directory_iterator(versionDir)) {
        if (entry.is_directory()) {
            std::string metadataPath = entry.path().string() + "/metadata.json";
            if (fs::exists(metadataPath)) {
                std::ifstream metaFile(metadataPath);
                std::stringstream buffer;
                buffer << metaFile.rdbuf();
                std::string content = buffer.str();
                
                std::string versionIdStr = "\"versionId\":" + std::to_string(versionId);
                if (content.find(versionIdStr) != std::string::npos) {
                    size_t pos = content.find(versionIdStr);
                    size_t versionPathPos = content.find("\"versionPath\":\"", pos);
                    if (versionPathPos != std::string::npos) {
                        versionPathPos += 15;
                        size_t endPos = content.find("\"", versionPathPos);
                        std::string versionPath = content.substr(versionPathPos, endPos - versionPathPos);
                        
                        try {
                            if (fs::exists(versionPath)) {
                                fs::remove(versionPath);
                            }
                            return "Success: Deleted version " + std::to_string(versionId) + "\n";
                        } catch (const fs::filesystem_error& e) {
                            return "Error: Failed to delete version: " + std::string(e.what()) + "\n";
                        }
                    }
                }
            }
        }
    }
    
    return "Error: Version not found\n";
}

std::string FileCommands::handlePreviewVersion(const std::string& args) {
    std::istringstream iss(args);
    int versionId;
    std::string filePath;
    iss >> versionId;
    std::getline(iss >> std::ws, filePath);
    
    namespace fs = std::filesystem;
    std::string versionDir;
    
    if (ctx_.daemonCore) {
        auto config = ctx_.daemonCore->getConfig();
        versionDir = config.watchDirectory + "/.sentinel_versions";
    }
    
    if (versionDir.empty() || !fs::exists(versionDir)) {
        return "Error: Version storage not found\n";
    }
    
    for (const auto& entry : fs::directory_iterator(versionDir)) {
        if (entry.is_directory()) {
            std::string metadataPath = entry.path().string() + "/metadata.json";
            if (fs::exists(metadataPath)) {
                std::ifstream metaFile(metadataPath);
                std::stringstream buffer;
                buffer << metaFile.rdbuf();
                std::string content = buffer.str();
                
                std::string versionIdStr = "\"versionId\":" + std::to_string(versionId);
                if (content.find(versionIdStr) != std::string::npos) {
                    size_t pos = content.find(versionIdStr);
                    size_t versionPathPos = content.find("\"versionPath\":\"", pos);
                    if (versionPathPos != std::string::npos) {
                        versionPathPos += 15;
                        size_t endPos = content.find("\"", versionPathPos);
                        std::string versionPath = content.substr(versionPathPos, endPos - versionPathPos);
                        
                        if (fs::exists(versionPath)) {
                            return "VERSION_PATH:" + versionPath + "\n";
                        }
                    }
                }
            }
        }
    }
    
    return "Error: Version not found\n";
}

} // namespace SentinelFS
