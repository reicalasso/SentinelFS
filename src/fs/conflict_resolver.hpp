#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <sys/stat.h>
#include <sys/file.h>  // For file locking
#include "../models.hpp"

enum class ConflictResolutionStrategy {
    TIMESTAMP,      // Use most recent timestamp
    LATEST,         // Always use latest version
    MERGE,          // Attempt to merge changes (for text files)
    ASK_USER,       // Ask user for resolution (not implemented in this version)
    BACKUP,         // Create backup of both versions
    P2P_VOTE        // Use voting from peers to decide
};

struct FileConflict {
    std::string filePath;
    std::string localVersion;
    std::string remoteVersion;
    std::string localTimestamp;
    std::string remoteTimestamp;
    std::vector<PeerInfo> conflictingPeers;
    std::chrono::system_clock::time_point detectedAt;
    
    FileConflict(const std::string& path) : filePath(path), detectedAt(std::chrono::system_clock::now()) {}
};

class ConflictResolver {
public:
    ConflictResolver(ConflictResolutionStrategy strategy = ConflictResolutionStrategy::TIMESTAMP);
    ~ConflictResolver();
    
    // Check if there's a conflict between local and remote versions
    bool checkConflict(const std::string& localFile, const std::string& remoteFile);
    
    // Resolve conflict using the configured strategy
    std::string resolveConflict(const FileConflict& conflict);
    
    // Apply resolution strategy
    std::string applyStrategy(const std::string& localFile, const std::string& remoteFile, 
                             const std::vector<PeerInfo>& peers = {});
    
    // Get file modification time
    static std::string getFileModificationTime(const std::string& filepath);
    
    // Backup conflicted file
    std::string createBackup(const std::string& filepath);
    
    // Set resolution strategy
    void setStrategy(ConflictResolutionStrategy strategy) { this->strategy = strategy; }
    
    // Register conflict for tracking
    void registerConflict(const FileConflict& conflict);
    
    // Get registered conflicts
    std::vector<FileConflict> getConflicts() const;
    
private:
    ConflictResolutionStrategy strategy;
    mutable std::mutex conflictMutex;
    std::vector<FileConflict> conflicts;
    
    // Strategy-specific methods
    std::string resolveByTimestamp(const std::string& localFile, const std::string& remoteFile);
    std::string resolveByLatest(const std::string& localFile, const std::string& remoteFile);
    std::string resolveByMerge(const std::string& localFile, const std::string& remoteFile);
    std::string resolveByBackup(const std::string& localFile, const std::string& remoteFile);
    std::string resolveByP2PVote(const std::string& localFile, const std::string& remoteFile, 
                                const std::vector<PeerInfo>& peers);
};