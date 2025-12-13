#include "ConflictHandler.h"
#include "DatabaseManager.h"
#include "Logger.h"
#include <sqlite3.h>
#include <algorithm>
#include <sstream>
#include <set>

namespace SentinelFS {
namespace Sync {

ConflictHandler::ConflictHandler(DatabaseManager* dbManager, IStorageAPI* storage)
    : dbManager_(dbManager), storage_(storage) {
    
    if (!dbManager_) {
        throw std::invalid_argument("Database manager cannot be null");
    }
    
    Logger::instance().log(LogLevel::INFO, "ConflictHandler initialized", "ConflictHandler");
}

ConflictHandler::~ConflictHandler() {
    Logger::instance().log(LogLevel::INFO, "ConflictHandler destroyed", "ConflictHandler");
}

std::vector<FileConflict> ConflictHandler::detectConflicts(
    const std::string& sessionId,
    const std::vector<SyncFile>& localFiles,
    const std::vector<SyncFile>& remoteFiles) {
    
    std::vector<FileConflict> conflicts;
    
    Logger::instance().log(LogLevel::DEBUG, 
        "Detecting conflicts for session: " + sessionId + 
        " (local: " + std::to_string(localFiles.size()) + 
        ", remote: " + std::to_string(remoteFiles.size()) + ")", 
        "ConflictHandler");
    
    // Create maps for efficient lookup
    std::map<std::string, SyncFile> localMap;
    std::map<std::string, SyncFile> remoteMap;
    
    for (const auto& file : localFiles) {
        localMap[file.filePath] = file;
    }
    
    for (const auto& file : remoteFiles) {
        remoteMap[file.filePath] = file;
    }
    
    // Find all unique file paths
    std::set<std::string> allPaths;
    for (const auto& pair : localMap) allPaths.insert(pair.first);
    for (const auto& pair : remoteMap) allPaths.insert(pair.first);
    
    // Check each file for conflicts
    for (const std::string& path : allPaths) {
        auto localIt = localMap.find(path);
        auto remoteIt = remoteMap.find(path);
        
        bool hasLocal = localIt != localMap.end();
        bool hasRemote = remoteIt != remoteMap.end();
        
        FileConflict conflict;
        conflict.filePath = path;
        conflict.status = ConflictStatus::PENDING;
        
        if (hasLocal && hasRemote) {
            // Both exist - check for modification conflicts
            const auto& local = localIt->second;
            const auto& remote = remoteIt->second;
            
            if (local.hash != remote.hash) {
                // Find common ancestor
                auto ancestors = findCommonAncestors(local.hash, remote.hash);
                std::string baseHash = ancestors.empty() ? "" : ancestors[0];
                
                // Get version information
                auto localVersion = getFileVersion(local.fileId, local.deviceId);
                auto remoteVersion = getFileVersion(remote.fileId, remote.deviceId);
                auto baseVersion = baseHash.empty() ? 0 : getFileVersionByHash(baseHash);
                
                conflict.fileId = local.fileId;
                conflict.localHash = local.hash;
                conflict.remoteHash = remote.hash;
                conflict.baseHash = baseHash;
                conflict.localVersion = localVersion;
                conflict.remoteVersion = remoteVersion;
                conflict.baseVersion = baseVersion;
                conflict.localDeviceId = local.deviceId;
                conflict.remoteDeviceId = remote.deviceId;
                conflict.localModifiedTime = local.modifiedTime;
                conflict.remoteModifiedTime = remote.modifiedTime;
                
                // Determine conflict type
                conflict.type = determineConflictType(local, remote, baseHash);
                
                if (conflict.type != ConflictType::NONE) {
                    conflicts.push_back(conflict);
                    
                    // Store in database
                    std::string conflictId = createConflictRecord(conflict);
                    
                    // Notify callback
                    if (onConflictDetected_) {
                        onConflictDetected_(conflict);
                    }
                }
            }
        } else if (hasLocal && !hasRemote) {
            // Local file deleted remotely or new locally
            const auto& local = localIt->second;
            
            // Check if this was a deletion conflict
            if (wasFileDeletedRemotely(local.fileId, local.deviceId)) {
                conflict.fileId = local.fileId;
                conflict.localHash = local.hash;
                conflict.remoteHash = "";
                conflict.baseHash = local.hash;
                conflict.localVersion = getFileVersion(local.fileId, local.deviceId);
                conflict.remoteVersion = 0;
                conflict.baseVersion = conflict.localVersion;
                conflict.localDeviceId = local.deviceId;
                conflict.remoteDeviceId = "";
                conflict.localModifiedTime = local.modifiedTime;
                conflict.remoteModifiedTime = 0;
                conflict.type = ConflictType::DELETION;
                
                conflicts.push_back(conflict);
                createConflictRecord(conflict);
                
                if (onConflictDetected_) {
                    onConflictDetected_(conflict);
                }
            }
        } else if (!hasLocal && hasRemote) {
            // Remote file deleted locally or new remotely
            const auto& remote = remoteIt->second;
            
            if (wasFileDeletedLocally(remote.fileId, remote.deviceId)) {
                conflict.fileId = remote.fileId;
                conflict.localHash = "";
                conflict.remoteHash = remote.hash;
                conflict.baseHash = remote.hash;
                conflict.localVersion = 0;
                conflict.remoteVersion = getFileVersion(remote.fileId, remote.deviceId);
                conflict.baseVersion = conflict.remoteVersion;
                conflict.localDeviceId = "";
                conflict.remoteDeviceId = remote.deviceId;
                conflict.localModifiedTime = 0;
                conflict.remoteModifiedTime = remote.modifiedTime;
                conflict.type = ConflictType::DELETION;
                
                conflicts.push_back(conflict);
                createConflictRecord(conflict);
                
                if (onConflictDetected_) {
                    onConflictDetected_(conflict);
                }
            }
        }
    }
    
    Logger::instance().log(LogLevel::INFO, 
        "Detected " + std::to_string(conflicts.size()) + " conflicts", 
        "ConflictHandler");
    
    return conflicts;
}

bool ConflictHandler::resolveConflict(
    const std::string& conflictId,
    const ConflictResolution& resolution) {
    
    try {
        auto conflict = getConflict(conflictId);
        if (!conflict) {
            Logger::instance().log(LogLevel::ERROR, 
                "Conflict not found: " + conflictId, 
                "ConflictHandler");
            return false;
        }
        
        bool success = false;
        
        switch (resolution.strategy) {
            case MergeStrategy::LOCAL_WINS:
                success = applyLocalVersion(conflictId);
                break;
                
            case MergeStrategy::REMOTE_WINS:
                success = applyRemoteVersion(conflictId);
                break;
                
            case MergeStrategy::AUTO_MERGE:
                success = performAutoMerge(conflictId);
                break;
                
            case MergeStrategy::MANUAL:
                success = applyMergedContent(conflictId, resolution.mergedContent);
                break;
                
            default:
                Logger::instance().log(LogLevel::WARN, 
                    "Unknown merge strategy", 
                    "ConflictHandler");
                return false;
        }
        
        if (success) {
            updateConflictStatus(conflictId, ConflictStatus::RESOLVED);
            
            // Create merge result record
            createMergeResultRecord(conflictId, resolution);
            
            if (onConflictResolved_) {
                onConflictResolved_(conflictId, true);
            }
            
            Logger::instance().log(LogLevel::INFO, 
                "Conflict resolved: " + conflictId, 
                "ConflictHandler");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        Logger::instance().log(LogLevel::ERROR, 
            "Failed to resolve conflict: " + std::string(e.what()), 
            "ConflictHandler");
        return false;
    }
}

std::vector<FileConflict> ConflictHandler::getPendingConflicts(const std::string& sessionId) {
    std::vector<FileConflict> conflicts;
    
    sqlite3* db = dbManager_->getDatabase();
    if (!db) return conflicts;
    
    const char* sql = R"(
        SELECT c.id, c.file_id, f.path, c.local_hash, c.remote_hash, c.base_hash,
               c.local_version, c.remote_version, c.base_version,
               c.conflict_type, c.status, c.local_device_id, c.remote_device_id,
               c.local_modified_time, c.remote_modified_time
        FROM conflicts c
        JOIN files f ON c.file_id = f.id
        WHERE c.status = 'pending'
        ORDER BY c.created_at DESC
    )";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FileConflict conflict;
            conflict.fileId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            conflict.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            conflict.localHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            conflict.remoteHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            conflict.baseHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            conflict.localVersion = sqlite3_column_int64(stmt, 6);
            conflict.remoteVersion = sqlite3_column_int64(stmt, 7);
            conflict.baseVersion = sqlite3_column_int64(stmt, 8);
            conflict.type = static_cast<ConflictType>(sqlite3_column_int(stmt, 9));
            conflict.status = ConflictStatus::PENDING;
            conflict.localDeviceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
            conflict.remoteDeviceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
            conflict.localModifiedTime = sqlite3_column_int64(stmt, 13);
            conflict.remoteModifiedTime = sqlite3_column_int64(stmt, 14);
            
            conflicts.push_back(conflict);
        }
    }
    
    if (stmt) sqlite3_finalize(stmt);
    return conflicts;
}

std::optional<FileConflict> ConflictHandler::getConflict(const std::string& conflictId) {
    sqlite3* db = dbManager_->getDatabase();
    if (!db) return std::nullopt;
    
    const char* sql = R"(
        SELECT c.file_id, f.path, c.local_hash, c.remote_hash, c.base_hash,
               c.local_version, c.remote_version, c.base_version,
               c.conflict_type, c.status, c.local_device_id, c.remote_device_id,
               c.local_modified_time, c.remote_modified_time
        FROM conflicts c
        JOIN files f ON c.file_id = f.id
        WHERE c.id = ?
    )";
    
    sqlite3_stmt* stmt = nullptr;
    FileConflict conflict;
    bool found = false;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, conflictId.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            conflict.fileId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            conflict.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            conflict.localHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            conflict.remoteHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            conflict.baseHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            conflict.localVersion = sqlite3_column_int64(stmt, 5);
            conflict.remoteVersion = sqlite3_column_int64(stmt, 6);
            conflict.baseVersion = sqlite3_column_int64(stmt, 7);
            conflict.type = static_cast<ConflictType>(sqlite3_column_int(stmt, 8));
            conflict.status = static_cast<ConflictStatus>(sqlite3_column_int(stmt, 9));
            conflict.localDeviceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
            conflict.remoteDeviceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
            conflict.localModifiedTime = sqlite3_column_int64(stmt, 12);
            conflict.remoteModifiedTime = sqlite3_column_int64(stmt, 13);
            
            found = true;
        }
    }
    
    if (stmt) sqlite3_finalize(stmt);
    
    return found ? std::optional<FileConflict>{conflict} : std::nullopt;
}

bool ConflictHandler::ignoreConflict(const std::string& conflictId) {
    try {
        updateConflictStatus(conflictId, ConflictStatus::IGNORED);
        
        if (onConflictResolved_) {
            onConflictResolved_(conflictId, false);
        }
        
        Logger::instance().log(LogLevel::INFO, 
            "Conflict ignored: " + conflictId, 
            "ConflictHandler");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::instance().log(LogLevel::ERROR, 
            "Failed to ignore conflict: " + std::string(e.what()), 
            "ConflictHandler");
        return false;
    }
}

bool ConflictHandler::autoResolveConflict(const std::string& conflictId) {
    auto conflict = getConflict(conflictId);
    if (!conflict) return false;
    
    // Try auto-merge first
    if (conflict->type == ConflictType::CONTENT) {
        MergeResult result = getMergePreview(conflictId);
        if (result.success && result.conflicts.empty()) {
            ConflictResolution resolution;
            resolution.conflictId = conflictId;
            resolution.strategy = MergeStrategy::AUTO_MERGE;
            resolution.mergedContent = result.mergedContent;
            resolution.resolved = true;
            
            return resolveConflict(conflictId, resolution);
        }
    }
    
    // For simple conflicts, use timestamp strategy
    if (conflict->type == ConflictType::METADATA) {
        ConflictResolution resolution;
        resolution.conflictId = conflictId;
        resolution.strategy = conflict->localModifiedTime > conflict->remoteModifiedTime ? 
                              MergeStrategy::LOCAL_WINS : MergeStrategy::REMOTE_WINS;
        resolution.resolved = true;
        
        return resolveConflict(conflictId, resolution);
    }
    
    return false;
}

MergeResult ConflictHandler::getMergePreview(const std::string& conflictId) {
    MergeResult result;
    
    auto conflict = getConflict(conflictId);
    if (!conflict) {
        result.errorMessage = "Conflict not found";
        return result;
    }
    
    if (conflict->type != ConflictType::CONTENT) {
        result.errorMessage = "Not a content conflict";
        return result;
    }
    
    // Get file contents
    std::string baseContent = getFileContent(conflict->filePath, conflict->baseHash);
    std::string localContent = getFileContent(conflict->filePath, conflict->localHash);
    std::string remoteContent = getFileContent(conflict->filePath, conflict->remoteHash);
    
    if (isBinaryFile(conflict->filePath)) {
        // Binary merge - use simple strategy
        std::vector<uint8_t> baseVec(baseContent.begin(), baseContent.end());
        std::vector<uint8_t> localVec(localContent.begin(), localContent.end());
        std::vector<uint8_t> remoteVec(remoteContent.begin(), remoteContent.end());
        
        result = MergeResolver::mergeBinary(baseVec, localVec, remoteVec, MergeStrategy::TIMESTAMP_WINS);
    } else {
        // Text merge - use 3-way merge
        result = MergeResolver::merge(baseContent, localContent, remoteContent, MergeStrategy::AUTO_MERGE);
    }
    
    return result;
}

void ConflictHandler::setConflictDetectedCallback(ConflictDetectedCallback callback) {
    onConflictDetected_ = callback;
}

void ConflictHandler::setConflictResolvedCallback(ConflictResolvedCallback callback) {
    onConflictResolved_ = callback;
}

// Private methods

std::string ConflictHandler::createConflictRecord(const FileConflict& conflict) {
    sqlite3* db = dbManager_->getDatabase();
    if (!db) return "";
    
    const char* sql = R"(
        INSERT INTO conflicts (
            file_id, device_id, conflict_type, local_version, remote_version,
            local_hash, remote_hash, status, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, 'pending', strftime('%s', 'now'), strftime('%s', 'now'))
    )";
    
    sqlite3_stmt* stmt = nullptr;
    std::string conflictId;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, conflict.fileId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, conflict.localDeviceId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, static_cast<int>(conflict.type));
        sqlite3_bind_int64(stmt, 4, conflict.localVersion);
        sqlite3_bind_int64(stmt, 5, conflict.remoteVersion);
        sqlite3_bind_text(stmt, 6, conflict.localHash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, conflict.remoteHash.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            conflictId = std::to_string(sqlite3_last_insert_rowid(db));
        }
    }
    
    if (stmt) sqlite3_finalize(stmt);
    return conflictId;
}

void ConflictHandler::updateConflictStatus(const std::string& conflictId, ConflictStatus status) {
    sqlite3* db = dbManager_->getDatabase();
    if (!db) return;
    
    const char* sql = R"(
        UPDATE conflicts 
        SET status = ?, updated_at = strftime('%s', 'now')
        WHERE id = ?
    )";
    
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, status == ConflictStatus::RESOLVED ? "resolved" : 
                              status == ConflictStatus::IGNORED ? "ignored" : "pending", 
                          -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, conflictId.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
    }
    
    if (stmt) sqlite3_finalize(stmt);
}

std::string ConflictHandler::getFileContent(const std::string& filePath, const std::string& hash) {
    // In a real implementation, this would retrieve content from storage
    // For now, return empty string
    return "";
}

std::vector<std::string> ConflictHandler::findCommonAncestors(
    const std::string& localHash,
    const std::string& remoteHash) {
    
    std::vector<std::string> ancestors;
    
    // Query database for common ancestors
    sqlite3* db = dbManager_->getDatabase();
    if (!db) return ancestors;
    
    const char* sql = R"(
        WITH RECURSIVE local_ancestors(hash) AS (
            SELECT hash FROM file_versions WHERE hash = ?
            UNION ALL
            SELECT parent_hash FROM file_versions v
            JOIN local_ancestors a ON v.hash = a.hash
            WHERE v.parent_hash IS NOT NULL
        ),
        remote_ancestors(hash) AS (
            SELECT hash FROM file_versions WHERE hash = ?
            UNION ALL
            SELECT parent_hash FROM file_versions v
            JOIN remote_ancestors a ON v.hash = a.hash
            WHERE v.parent_hash IS NOT NULL
        )
        SELECT hash FROM local_ancestors
        INTERSECT
        SELECT hash FROM remote_ancestors
        ORDER BY version DESC
        LIMIT 1
    )";
    
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, localHash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, remoteHash.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            ancestors.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
    }
    
    if (stmt) sqlite3_finalize(stmt);
    return ancestors;
}

bool ConflictHandler::isBinaryFile(const std::string& filePath) {
    // Simple heuristic based on file extension
    std::string ext = filePath.substr(filePath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    static const std::set<std::string> binaryExtensions = {
        "jpg", "jpeg", "png", "gif", "bmp", "ico",
        "mp3", "mp4", "avi", "mov", "wav",
        "zip", "rar", "tar", "gz", "7z",
        "exe", "dll", "so", "dylib",
        "pdf", "doc", "docx", "xls", "xlsx"
    };
    
    return binaryExtensions.count(ext) > 0;
}

ConflictType ConflictHandler::determineConflictType(
    const SyncFile& local,
    const SyncFile& remote,
    const std::string& baseHash) {
    
    if (local.deleted != remote.deleted) {
        return ConflictType::DELETION;
    }
    
    if (local.filePath != remote.filePath) {
        return ConflictType::RENAME;
    }
    
    if (local.hash != remote.hash) {
        return ConflictType::CONTENT;
    }
    
    if (local.size != remote.size || local.modifiedTime != remote.modifiedTime) {
        return ConflictType::METADATA;
    }
    
    return ConflictType::NONE;
}

// Private helper method stubs
uint64_t ConflictHandler::getFileVersion(const std::string& fileId, const std::string& deviceId) {
    // Stub implementation - would query file_versions table
    return 1;
}

uint64_t ConflictHandler::getFileVersionByHash(const std::string& hash) {
    // Stub implementation - would query file_versions table
    return 1;
}

bool ConflictHandler::wasFileDeletedRemotely(const std::string& fileId, const std::string& deviceId) {
    // Stub implementation - would check sync_state for deletion
    return false;
}

bool ConflictHandler::wasFileDeletedLocally(const std::string& fileId, const std::string& deviceId) {
    // Stub implementation - would check sync_state for deletion
    return false;
}

bool ConflictHandler::applyLocalVersion(const std::string& conflictId) {
    // Stub implementation - would apply local version to resolve conflict
    return true;
}

bool ConflictHandler::applyRemoteVersion(const std::string& conflictId) {
    // Stub implementation - would apply remote version to resolve conflict
    return true;
}

bool ConflictHandler::performAutoMerge(const std::string& conflictId) {
    // Stub implementation - would perform automatic merge
    return true;
}

bool ConflictHandler::applyMergedContent(const std::string& conflictId, const std::string& mergedContent) {
    // Stub implementation - would apply merged content
    return true;
}

void ConflictHandler::createMergeResultRecord(const std::string& conflictId, const ConflictResolution& resolution) {
    // Stub implementation - would create record in merge_results table
}

} // namespace Sync
} // namespace SentinelFS
