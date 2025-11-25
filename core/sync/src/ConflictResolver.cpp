#include "ConflictResolver.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace SentinelFS {

bool ConflictResolver::detectConflict(
    const std::string& path,
    const std::string& localHash,
    const std::string& remoteHash,
    uint64_t localTimestamp,
    uint64_t remoteTimestamp,
    const VectorClock& localClock,
    const VectorClock& remoteClock
) {
    // No conflict if hashes match
    if (localHash == remoteHash) {
        return false;
    }
    
    // Check vector clocks for causality
    if (remoteClock.happensBefore(localClock)) {
        // Remote is older, local version supersedes it
        std::cout << "No conflict: Remote change is causally older" << std::endl;
        return false;
    }
    
    if (localClock.happensBefore(remoteClock)) {
        // Local is older, remote version supersedes it
        std::cout << "No conflict: Local change is causally older" << std::endl;
        return false;
    }
    
    // Concurrent modifications detected
    if (remoteClock.isConcurrentWith(localClock)) {
        std::cout << "⚠️  CONFLICT DETECTED: Concurrent modifications on " << path << std::endl;
        std::cout << "   Local timestamp: " << localTimestamp << ", Remote timestamp: " << remoteTimestamp << std::endl;
        return true;
    }
    
    return false;
}

bool ConflictResolver::resolveConflict(
    FileConflict& conflict,
    const std::string& localPath,
    const std::vector<uint8_t>& remoteData
) {
    std::cout << "Resolving conflict for " << conflict.path 
              << " using strategy: " << static_cast<int>(conflict.strategy) << std::endl;
    
    bool success = false;
    
    switch (conflict.strategy) {
        case ResolutionStrategy::NEWEST_WINS:
            success = applyNewestWins(conflict, localPath, remoteData);
            break;
            
        case ResolutionStrategy::KEEP_BOTH:
            success = applyKeepBoth(conflict, localPath, remoteData);
            break;
            
        case ResolutionStrategy::LARGEST_WINS:
            success = applyLargestWins(conflict, localPath, remoteData);
            break;
            
        case ResolutionStrategy::REMOTE_WINS:
            // Always accept remote version
            try {
                std::ofstream file(localPath, std::ios::binary);
                file.write(reinterpret_cast<const char*>(remoteData.data()), remoteData.size());
                file.close();
                success = true;
                std::cout << "✓ Applied REMOTE_WINS: Overwrote local with remote" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to apply REMOTE_WINS: " << e.what() << std::endl;
            }
            break;
            
        case ResolutionStrategy::LOCAL_WINS:
            // Keep local version, ignore remote
            success = true;
            std::cout << "✓ Applied LOCAL_WINS: Kept local version" << std::endl;
            break;
            
        case ResolutionStrategy::MANUAL:
            // Mark for manual resolution (save both versions)
            success = applyKeepBoth(conflict, localPath, remoteData);
            std::cout << "⚠️  MANUAL resolution required. Both versions saved." << std::endl;
            break;
    }
    
    if (success) {
        conflict.resolved = true;
    }
    
    return success;
}

bool ConflictResolver::applyNewestWins(
    const FileConflict& conflict,
    const std::string& localPath,
    const std::vector<uint8_t>& remoteData
) {
    try {
        if (conflict.remoteTimestamp > conflict.localTimestamp) {
            // Remote is newer, overwrite local
            std::ofstream file(localPath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(remoteData.data()), remoteData.size());
            file.close();
            std::cout << "✓ Applied NEWEST_WINS: Remote version is newer" << std::endl;
        } else {
            // Local is newer or equal, keep it
            std::cout << "✓ Applied NEWEST_WINS: Local version is newer/equal" << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to apply NEWEST_WINS: " << e.what() << std::endl;
        return false;
    }
}

bool ConflictResolver::applyKeepBoth(
    const FileConflict& conflict,
    const std::string& localPath,
    const std::vector<uint8_t>& remoteData
) {
    try {
        namespace fs = std::filesystem;
        
        // Generate timestamp suffix
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        struct tm tm_buf;
        localtime_r(&time_t, &tm_buf);
        ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
        std::string timestamp = ss.str();
        
        // Save local version with .conflict.local suffix
        std::string localConflictPath = generateConflictPath(localPath, "local_" + timestamp);
        if (fs::exists(localPath)) {
            fs::copy_file(localPath, localConflictPath, fs::copy_options::overwrite_existing);
            std::cout << "✓ Saved local version: " << localConflictPath << std::endl;
        }
        
        // Save remote version with .conflict.remote suffix
        std::string remoteConflictPath = generateConflictPath(
            localPath, 
            "remote_" + conflict.remotePeerId + "_" + timestamp
        );
        std::ofstream remoteFile(remoteConflictPath, std::ios::binary);
        remoteFile.write(reinterpret_cast<const char*>(remoteData.data()), remoteData.size());
        remoteFile.close();
        std::cout << "✓ Saved remote version: " << remoteConflictPath << std::endl;
        
        // Keep local as-is (user decides which to use)
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to apply KEEP_BOTH: " << e.what() << std::endl;
        return false;
    }
}

bool ConflictResolver::applyLargestWins(
    const FileConflict& conflict,
    const std::string& localPath,
    const std::vector<uint8_t>& remoteData
) {
    try {
        if (conflict.remoteSize > conflict.localSize) {
            // Remote is larger, overwrite local
            std::ofstream file(localPath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(remoteData.data()), remoteData.size());
            file.close();
            std::cout << "✓ Applied LARGEST_WINS: Remote version is larger (" 
                      << conflict.remoteSize << " > " << conflict.localSize << " bytes)" << std::endl;
        } else {
            // Local is larger or equal, keep it
            std::cout << "✓ Applied LARGEST_WINS: Local version is larger/equal" << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to apply LARGEST_WINS: " << e.what() << std::endl;
        return false;
    }
}

std::string ConflictResolver::generateConflictPath(
    const std::string& originalPath,
    const std::string& suffix
) {
    namespace fs = std::filesystem;
    fs::path p(originalPath);
    
    std::string stem = p.stem().string();
    std::string ext = p.extension().string();
    std::string dir = p.parent_path().string();
    
    std::string conflictName = stem + ".conflict." + suffix + ext;
    
    if (dir.empty()) {
        return conflictName;
    } else {
        return dir + "/" + conflictName;
    }
}

} // namespace SentinelFS
