#include "snapshot_engine.h"
#include "../logger/logger.h"
#include <algorithm>

namespace sfs {
namespace core {

// ============================================================================
// Snapshot Implementation
// ============================================================================

void Snapshot::add_file(const std::string& path, const FileInfo& info) {
    files_[path] = info;
}

const FileInfo* Snapshot::get_file(const std::string& path) const {
    auto it = files_.find(path);
    return (it != files_.end()) ? &it->second : nullptr;
}

bool Snapshot::has_file(const std::string& path) const {
    return files_.find(path) != files_.end();
}

std::vector<std::string> Snapshot::get_all_paths() const {
    std::vector<std::string> paths;
    paths.reserve(files_.size());
    
    for (const auto& [path, _] : files_) {
        paths.push_back(path);
    }
    
    return paths;
}

// ============================================================================
// SnapshotEngine Implementation
// ============================================================================

SnapshotEngine::SnapshotEngine(std::shared_ptr<IFileAPI> file_api)
    : file_api_(std::move(file_api)) {
    
    // Default ignore patterns
    ignore_patterns_ = {
        ".git",
        ".gitignore",
        ".svn",
        "node_modules",
        "__pycache__",
        "*.tmp",
        "*.swp",
        ".DS_Store",
        "thumbs.db"
    };
}

SnapshotEngine::~SnapshotEngine() {
}

Snapshot SnapshotEngine::create_snapshot(
    const std::string& root_path,
    const std::vector<std::string>& ignore_patterns
) {
    Snapshot snapshot;
    
    if (!file_api_->exists(root_path)) {
        SFS_LOG_ERROR("SnapshotEngine", "Root path does not exist: " + root_path);
        return snapshot;
    }
    
    if (!file_api_->is_directory(root_path)) {
        SFS_LOG_ERROR("SnapshotEngine", "Root path is not a directory: " + root_path);
        return snapshot;
    }
    
    // Temporarily override ignore patterns if provided
    auto old_patterns = ignore_patterns_;
    if (!ignore_patterns.empty()) {
        ignore_patterns_ = ignore_patterns;
    }
    
    SFS_LOG_INFO("SnapshotEngine", "Creating snapshot of: " + root_path);
    
    // Recursively scan directory
    scan_directory(root_path, snapshot);
    
    // Restore patterns
    ignore_patterns_ = old_patterns;
    
    SFS_LOG_INFO("SnapshotEngine", "Snapshot created with " + 
                 std::to_string(snapshot.file_count()) + " files");
    
    return snapshot;
}

SnapshotComparison SnapshotEngine::compare_snapshots(
    const Snapshot& old_snapshot,
    const Snapshot& new_snapshot
) {
    SnapshotComparison result;
    
    auto old_paths = old_snapshot.get_all_paths();
    auto new_paths = new_snapshot.get_all_paths();
    
    // Create sets for efficient lookup
    std::set<std::string> old_set(old_paths.begin(), old_paths.end());
    std::set<std::string> new_set(new_paths.begin(), new_paths.end());
    
    // Find added files (in new but not in old)
    for (const auto& path : new_paths) {
        if (old_set.find(path) == old_set.end()) {
            FileChange change(ChangeType::ADDED, path);
            const FileInfo* info = new_snapshot.get_file(path);
            if (info) {
                change.new_info = *info;
            }
            result.changes.push_back(std::move(change));
        }
    }
    
    // Find removed files (in old but not in new)
    for (const auto& path : old_paths) {
        if (new_set.find(path) == new_set.end()) {
            FileChange change(ChangeType::REMOVED, path);
            const FileInfo* info = old_snapshot.get_file(path);
            if (info) {
                change.old_info = *info;
            }
            result.changes.push_back(std::move(change));
        }
    }
    
    // Find modified files (in both but different)
    for (const auto& path : new_paths) {
        if (old_set.find(path) != old_set.end()) {
            const FileInfo* old_info = old_snapshot.get_file(path);
            const FileInfo* new_info = new_snapshot.get_file(path);
            
            if (old_info && new_info) {
                // Compare by hash (most reliable)
                if (old_info->hash != new_info->hash) {
                    FileChange change(ChangeType::MODIFIED, path);
                    change.old_info = *old_info;
                    change.new_info = *new_info;
                    result.changes.push_back(std::move(change));
                }
                // Also check size and modification time as quick checks
                else if (old_info->size != new_info->size ||
                         old_info->modified_time != new_info->modified_time) {
                    FileChange change(ChangeType::MODIFIED, path);
                    change.old_info = *old_info;
                    change.new_info = *new_info;
                    result.changes.push_back(std::move(change));
                }
            }
        }
    }
    
    SFS_LOG_INFO("SnapshotEngine", "Comparison: " +
                 std::to_string(result.added_count()) + " added, " +
                 std::to_string(result.removed_count()) + " removed, " +
                 std::to_string(result.modified_count()) + " modified");
    
    return result;
}

void SnapshotEngine::set_ignore_patterns(const std::vector<std::string>& patterns) {
    ignore_patterns_ = patterns;
}

bool SnapshotEngine::should_ignore(const std::string& path) const {
    for (const auto& pattern : ignore_patterns_) {
        // Simple pattern matching (can be enhanced with regex)
        if (path.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void SnapshotEngine::scan_directory(
    const std::string& path,
    Snapshot& snapshot
) {
    if (should_ignore(path)) {
        return;
    }
    
    try {
        auto entries = file_api_->list_directory(path, false);
        
        for (const auto& entry : entries) {
            if (should_ignore(entry)) {
                continue;
            }
            
            if (file_api_->is_directory(entry)) {
                // Recursively scan subdirectory
                scan_directory(entry, snapshot);
            } else {
                // Add file to snapshot
                FileInfo info = file_api_->get_file_info(entry);
                if (!info.path.empty()) {
                    snapshot.add_file(entry, info);
                }
            }
        }
        
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("SnapshotEngine", "Failed to scan directory " + 
                      path + ": " + e.what());
    }
}

} // namespace core
} // namespace sfs
