#ifndef SFS_SNAPSHOT_ENGINE_H
#define SFS_SNAPSHOT_ENGINE_H

#include "file_api.h"
#include <map>
#include <memory>
#include <set>

namespace sfs {
namespace core {

/**
 * @brief Snapshot - Represents file system state at a point in time
 * 
 * Contains a map of file paths to their metadata.
 * Used for detecting changes between two points in time.
 */
class Snapshot {
public:
    Snapshot() = default;
    
    /**
     * @brief Add file to snapshot
     */
    void add_file(const std::string& path, const FileInfo& info);
    
    /**
     * @brief Get file info
     */
    const FileInfo* get_file(const std::string& path) const;
    
    /**
     * @brief Check if file exists in snapshot
     */
    bool has_file(const std::string& path) const;
    
    /**
     * @brief Get all file paths
     */
    std::vector<std::string> get_all_paths() const;
    
    /**
     * @brief Get file count
     */
    size_t file_count() const { return files_.size(); }
    
    /**
     * @brief Clear snapshot
     */
    void clear() { files_.clear(); }

private:
    std::map<std::string, FileInfo> files_;
};

/**
 * @brief ChangeType - Types of file changes
 */
enum class ChangeType {
    ADDED,      // New file
    REMOVED,    // Deleted file
    MODIFIED    // Content or metadata changed
};

/**
 * @brief FileChange - Represents a detected change
 */
struct FileChange {
    ChangeType type;
    std::string path;
    FileInfo old_info;  // Valid for REMOVED and MODIFIED
    FileInfo new_info;  // Valid for ADDED and MODIFIED
    
    FileChange(ChangeType t, const std::string& p)
        : type(t), path(p) {}
};

/**
 * @brief SnapshotComparison - Result of comparing two snapshots
 */
struct SnapshotComparison {
    std::vector<FileChange> changes;
    
    size_t added_count() const {
        size_t count = 0;
        for (const auto& c : changes) {
            if (c.type == ChangeType::ADDED) count++;
        }
        return count;
    }
    
    size_t removed_count() const {
        size_t count = 0;
        for (const auto& c : changes) {
            if (c.type == ChangeType::REMOVED) count++;
        }
        return count;
    }
    
    size_t modified_count() const {
        size_t count = 0;
        for (const auto& c : changes) {
            if (c.type == ChangeType::MODIFIED) count++;
        }
        return count;
    }
    
    bool has_changes() const {
        return !changes.empty();
    }
};

/**
 * @brief SnapshotEngine - Directory scanning and change detection
 * 
 * Core component for:
 * - Capturing filesystem state
 * - Detecting changes between snapshots
 * - Preparing sync tasks
 * 
 * This is NOT a plugin - it's part of Core infrastructure.
 */
class SnapshotEngine {
public:
    /**
     * @brief Constructor
     * 
     * @param file_api File API implementation
     */
    explicit SnapshotEngine(std::shared_ptr<IFileAPI> file_api);
    
    ~SnapshotEngine();
    
    /**
     * @brief Create snapshot of directory
     * 
     * Recursively scans directory and captures all file metadata.
     * 
     * @param root_path Root directory to scan
     * @param ignore_patterns File patterns to ignore (e.g., ".git", "*.tmp")
     * @return Snapshot object
     */
    Snapshot create_snapshot(
        const std::string& root_path,
        const std::vector<std::string>& ignore_patterns = {}
    );
    
    /**
     * @brief Compare two snapshots
     * 
     * Detects:
     * - Added files (in new but not in old)
     * - Removed files (in old but not in new)
     * - Modified files (different hash or metadata)
     * 
     * @param old_snapshot Previous snapshot
     * @param new_snapshot Current snapshot
     * @return Comparison result with all changes
     */
    SnapshotComparison compare_snapshots(
        const Snapshot& old_snapshot,
        const Snapshot& new_snapshot
    );
    
    /**
     * @brief Set ignore patterns
     * 
     * Files matching these patterns will be skipped during scanning.
     * 
     * @param patterns List of patterns (glob-style)
     */
    void set_ignore_patterns(const std::vector<std::string>& patterns);
    
    /**
     * @brief Get ignore patterns
     */
    const std::vector<std::string>& get_ignore_patterns() const {
        return ignore_patterns_;
    }

private:
    std::shared_ptr<IFileAPI> file_api_;
    std::vector<std::string> ignore_patterns_;
    
    /**
     * @brief Check if path should be ignored
     */
    bool should_ignore(const std::string& path) const;
    
    /**
     * @brief Recursively scan directory
     */
    void scan_directory(
        const std::string& path,
        Snapshot& snapshot
    );
};

} // namespace core
} // namespace sfs

#endif // SFS_SNAPSHOT_ENGINE_H
