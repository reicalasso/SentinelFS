#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <set>
#include <regex>
#include "../models.hpp"

// Sync priority levels
enum class SyncPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// Conflict resolution strategies
enum class ConflictResolutionStrategy {
    LATEST_WINS,      // Most recent version
    OLDEST_WINS,      // Oldest version
    LARGEST_WINS,     // Largest file size
    SMALLEST_WINS,    // Smallest file size
    PEER_PRIORITY,    // Based on peer priority
    USER_DECISION,    // Ask user to decide
    MERGE_FILES       // Merge conflicting versions (text files)
};

// Sync rule for selective sync
struct SyncRule {
    std::string pattern;              // File pattern (glob or regex)
    SyncPriority priority;            // Sync priority for matching files
    bool include;                     // Include or exclude matching files
    std::chrono::hours activeHours;  // Hours when rule is active (0-23)
    size_t maxSize;                  // Maximum file size to sync (0 = unlimited)
    std::vector<std::string> tags;    // Custom tags for rule categorization
    
    SyncRule() : priority(SyncPriority::NORMAL), include(true), 
                 activeHours(0), maxSize(0) {}
    
    SyncRule(const std::string& patt, SyncPriority prio = SyncPriority::NORMAL, bool incl = true)
        : pattern(patt), priority(prio), include(incl), activeHours(0), maxSize(0) {}
};

// Selective sync manager
class SelectiveSyncManager {
public:
    SelectiveSyncManager();
    ~SelectiveSyncManager();
    
    // Rule management
    void addSyncRule(const SyncRule& rule);
    void removeSyncRule(const std::string& pattern);
    void clearSyncRules();
    std::vector<SyncRule> getSyncRules() const;
    
    // Pattern matching
    bool shouldSyncFile(const std::string& filePath, size_t fileSize = 0) const;
    SyncPriority getFilePriority(const std::string& filePath) const;
    std::vector<SyncRule> getMatchingRules(const std::string& filePath) const;
    
    // Conflict resolution
    void addConflictResolutionRule(const std::string& pattern, ConflictResolutionStrategy strategy);
    ConflictResolutionStrategy getConflictResolutionStrategy(const std::string& filePath) const;
    
    // Priority management
    void setDefaultPriority(SyncPriority priority);
    SyncPriority getDefaultPriority() const;
    
    // File type based priorities
    void setFileTypePriority(const std::string& fileType, SyncPriority priority);
    SyncPriority getFileTypePriority(const std::string& fileType) const;
    
    // Time-based sync
    void setSyncHours(const std::vector<int>& hours);
    std::vector<int> getSyncHours() const;
    bool isActiveHour() const;
    
    // Size-based sync
    void setMaxSyncFileSize(size_t maxSize);
    size_t getMaxSyncFileSize() const;
    
    // Tag-based sync
    void addFileTag(const std::string& filePath, const std::string& tag);
    void removeFileTag(const std::string& filePath, const std::string& tag);
    bool hasTag(const std::string& filePath, const std::string& tag) const;
    std::set<std::string> getFileTags(const std::string& filePath) const;
    
    // Pattern utilities
    bool matchesPattern(const std::string& filePath, const std::string& pattern) const;
    bool isGlobPattern(const std::string& pattern) const;
    bool isRegexPattern(const std::string& pattern) const;
    
    // Statistics
    size_t getRuleCount() const;
    size_t getSyncedFileCount() const;
    double getSyncEfficiency() const;
    
    // Validation
    bool isValidRule(const SyncRule& rule) const;
    std::string validatePattern(const std::string& pattern) const;
    
private:
    mutable std::mutex rulesMutex;
    std::vector<SyncRule> syncRules;
    SyncPriority defaultPriority;
    
    mutable std::mutex conflictMutex;
    std::map<std::string, ConflictResolutionStrategy> conflictResolutionRules;
    
    mutable std::mutex timeMutex;
    std::vector<int> syncHours;
    
    mutable std::mutex sizeMutex;
    size_t maxSyncFileSize;
    
    mutable std::mutex tagsMutex;
    std::map<std::string, std::set<std::string>> fileTags;
    
    mutable std::mutex statsMutex;
    size_t syncedFiles;
    size_t totalSyncAttempts;
    
    // Internal helpers
    bool matchesGlobPattern(const std::string& filePath, const std::string& pattern) const;
    bool matchesRegexPattern(const std::string& filePath, const std::string& pattern) const;
    SyncPriority getHighestPriority(const std::vector<SyncRule>& rules) const;
    bool isWithinActiveHours(const SyncRule& rule) const;
    std::string getFileExtension(const std::string& filePath) const;
    std::string getFileName(const std::string& filePath) const;
    
    // Performance optimizations
    mutable std::mutex cacheMutex;
    mutable std::map<std::string, bool> patternMatchCache;
    mutable std::map<std::string, SyncPriority> priorityCache;
    mutable std::chrono::steady_clock::time_point lastCacheClear;
    
    void clearCacheIfExpired();
};