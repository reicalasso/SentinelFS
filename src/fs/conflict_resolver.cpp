#include "conflict_resolver.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <chrono>
#include <thread>
#include <cstring>

ConflictResolver::ConflictResolver(ConflictResolutionStrategy strategy) 
    : strategy(strategy) {}

ConflictResolver::~ConflictResolver() = default;

bool ConflictResolver::checkConflict(const std::string& localFile, const std::string& remoteFile) {
    // Check if both files exist and have different content
    std::ifstream localFileStream(localFile, std::ios::binary);
    std::ifstream remoteFileStream(remoteFile, std::ios::binary);
    
    if (!localFileStream || !remoteFileStream) {
        return false; // One or both files don't exist
    }
    
    // Get file sizes
    localFileStream.seekg(0, std::ios::end);
    remoteFileStream.seekg(0, std::ios::end);
    
    size_t localSize = localFileStream.tellg();
    size_t remoteSize = remoteFileStream.tellg();
    
    // If sizes differ, there's likely a conflict
    if (localSize != remoteSize) {
        return true;
    }
    
    // If same size, compare content
    localFileStream.seekg(0, std::ios::beg);
    remoteFileStream.seekg(0, std::ios::beg);
    
    const size_t bufferSize = 8192;
    std::vector<char> localBuffer(bufferSize);
    std::vector<char> remoteBuffer(bufferSize);
    
    while (localFileStream && remoteFileStream) {
        localFileStream.read(localBuffer.data(), bufferSize);
        remoteFileStream.read(remoteBuffer.data(), bufferSize);
        
        size_t localRead = localFileStream.gcount();
        size_t remoteRead = remoteFileStream.gcount();
        
        if (localRead != remoteRead || 
            memcmp(localBuffer.data(), remoteBuffer.data(), std::min(localRead, remoteRead)) != 0) {
            return true;
        }
    }
    
    return false;
}

std::string ConflictResolver::resolveConflict(const FileConflict& conflict) {
    // Register the conflict first
    registerConflict(conflict);
    
    // Apply the configured strategy
    return applyStrategy(conflict.localVersion, conflict.remoteVersion, conflict.conflictingPeers);
}

std::string ConflictResolver::applyStrategy(const std::string& localFile, const std::string& remoteFile, 
                                          const std::vector<PeerInfo>& peers) {
    switch (strategy) {
        case ConflictResolutionStrategy::TIMESTAMP:
            return resolveByTimestamp(localFile, remoteFile);
        case ConflictResolutionStrategy::LATEST:
            return resolveByLatest(localFile, remoteFile);
        case ConflictResolutionStrategy::MERGE:
            return resolveByMerge(localFile, remoteFile);
        case ConflictResolutionStrategy::BACKUP:
            return resolveByBackup(localFile, remoteFile);
        case ConflictResolutionStrategy::P2P_VOTE:
            return resolveByP2PVote(localFile, remoteFile, peers);
        default:
            return resolveByTimestamp(localFile, remoteFile);
    }
}

std::string ConflictResolver::getFileModificationTime(const std::string& filepath) {
    struct stat fileStat;
    if (stat(filepath.c_str(), &fileStat) == 0) {
        std::stringstream ss;
        ss << std::to_string(fileStat.st_mtime);
        return ss.str();
    }
    return "";
}

std::string ConflictResolver::createBackup(const std::string& filepath) {
    std::string timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    std::string backupPath = filepath + ".backup_" + timestamp;
    
    // Copy the file to backup location
    std::ifstream src(filepath, std::ios::binary);
    std::ofstream dst(backupPath, std::ios::binary);
    
    if (src && dst) {
        dst << src.rdbuf();
        return backupPath;
    }
    
    return ""; // Backup creation failed
}

void ConflictResolver::registerConflict(const FileConflict& conflict) {
    std::lock_guard<std::mutex> lock(conflictMutex);
    conflicts.push_back(conflict);
}

std::vector<FileConflict> ConflictResolver::getConflicts() const {
    std::lock_guard<std::mutex> lock(conflictMutex);
    return conflicts;
}

std::string ConflictResolver::resolveByTimestamp(const std::string& localFile, const std::string& remoteFile) {
    std::string localTime = getFileModificationTime(localFile);
    std::string remoteTime = getFileModificationTime(remoteFile);
    
    if (localTime.empty() || remoteTime.empty()) {
        // If we can't get timestamps, default to remote (newer)
        return remoteFile;
    }
    
    if (localTime > remoteTime) {
        // Local is more recent
        return localFile;
    } else {
        // Remote is more recent or equal
        return remoteFile;
    }
}

std::string ConflictResolver::resolveByLatest(const std::string& localFile, const std::string& remoteFile) {
    // Always use the remote version (assumes it's the latest)
    return remoteFile;
}

std::string ConflictResolver::resolveByMerge(const std::string& localFile, const std::string& remoteFile) {
    // For text files, we could implement a basic line-by-line merge
    // For now, return a merged version by appending changes
    std::ifstream localStream(localFile);
    std::ifstream remoteStream(remoteFile);
    
    if (!localStream || !remoteStream) {
        // If files can't be read, default to remote
        return remoteFile;
    }
    
    // Read both files
    std::string localContent((std::istreambuf_iterator<char>(localStream)),
                             std::istreambuf_iterator<char>());
    std::string remoteContent((std::istreambuf_iterator<char>(remoteStream)),
                              std::istreambuf_iterator<char>());
    
    // Create a merge result - for now, we'll create a unique filename for the merged result
    std::string mergedFile = localFile + ".merged";
    std::ofstream mergedStream(mergedFile);
    
    if (mergedStream) {
        // Write content from local file
        mergedStream << localContent;
        
        // Write content from remote file if it's different
        if (localContent != remoteContent) {
            mergedStream << "\n--- MERGED FROM REMOTE ---\n" << remoteContent;
        }
        
        return mergedFile;
    }
    
    // If merge failed, default to remote
    return remoteFile;
}

std::string ConflictResolver::resolveByBackup(const std::string& localFile, const std::string& remoteFile) {
    // Create backups of both files
    std::string localBackup = createBackup(localFile);
    std::string remoteBackup = createBackup(remoteFile);
    
    // Use remote as the "current" version
    // Return the remote file but ensure backups are created
    (void)localBackup;  // Prevent unused variable warning
    (void)remoteBackup; // Prevent unused variable warning
    
    return remoteFile;
}

std::string ConflictResolver::resolveByP2PVote(const std::string& localFile, const std::string& remoteFile, 
                                              const std::vector<PeerInfo>& peers) {
    // In a real implementation, this would query peers for their versions and vote
    // For now, we'll just return remote file
    (void)peers; // Prevent unused variable warning
    
    // In a real P2P voting system, we would:
    // 1. Query other peers for their version of the file
    // 2. Count votes for local vs remote version
    // 3. Return the version with the most votes
    
    return remoteFile;
}