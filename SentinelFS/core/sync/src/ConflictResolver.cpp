#include "ConflictResolver.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>
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
    
    auto& logger = Logger::instance();
    
    // Check vector clocks for causality
    if (remoteClock.happensBefore(localClock)) {
        // Remote is older, local version supersedes it
        logger.debug("No conflict: Remote change is causally older", "ConflictResolver");
        return false;
    }
    
    if (localClock.happensBefore(remoteClock)) {
        // Local is older, remote version supersedes it
        logger.debug("No conflict: Local change is causally older", "ConflictResolver");
        return false;
    }
    
    // Concurrent modifications detected
    if (remoteClock.isConcurrentWith(localClock)) {
        logger.warn("CONFLICT DETECTED: Concurrent modifications on " + path, "ConflictResolver");
        logger.debug("Local timestamp: " + std::to_string(localTimestamp) + ", Remote: " + std::to_string(remoteTimestamp), "ConflictResolver");
        return true;
    }
    
    return false;
}

bool ConflictResolver::resolveConflict(
    FileConflict& conflict,
    const std::string& localPath,
    const std::vector<uint8_t>& remoteData
) {
    auto& logger = Logger::instance();
    logger.info("Resolving conflict for " + conflict.path + " using strategy: " + std::to_string(static_cast<int>(conflict.strategy)), "ConflictResolver");
    
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
                logger.info("Applied REMOTE_WINS: Overwrote local with remote", "ConflictResolver");
            } catch (const std::exception& e) {
                logger.error("Failed to apply REMOTE_WINS: " + std::string(e.what()), "ConflictResolver");
            }
            break;
            
        case ResolutionStrategy::LOCAL_WINS:
            // Keep local version, ignore remote
            success = true;
            logger.info("Applied LOCAL_WINS: Kept local version", "ConflictResolver");
            break;
            
        case ResolutionStrategy::MANUAL:
            // Mark for manual resolution (save both versions)
            success = applyKeepBoth(conflict, localPath, remoteData);
            logger.warn("MANUAL resolution required. Both versions saved.", "ConflictResolver");
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
    auto& logger = Logger::instance();
    try {
        if (conflict.remoteTimestamp > conflict.localTimestamp) {
            // Remote is newer, overwrite local
            std::ofstream file(localPath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(remoteData.data()), remoteData.size());
            file.close();
            logger.info("Applied NEWEST_WINS: Remote version is newer", "ConflictResolver");
        } else {
            // Local is newer or equal, keep it
            logger.info("Applied NEWEST_WINS: Local version is newer/equal", "ConflictResolver");
        }
        return true;
    } catch (const std::exception& e) {
        logger.error("Failed to apply NEWEST_WINS: " + std::string(e.what()), "ConflictResolver");
        return false;
    }
}

bool ConflictResolver::applyKeepBoth(
    const FileConflict& conflict,
    const std::string& localPath,
    const std::vector<uint8_t>& remoteData
) {
    auto& logger = Logger::instance();
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
            logger.info("Saved local version: " + localConflictPath, "ConflictResolver");
        }
        
        // Save remote version with .conflict.remote suffix
        std::string remoteConflictPath = generateConflictPath(
            localPath, 
            "remote_" + conflict.remotePeerId + "_" + timestamp
        );
        std::ofstream remoteFile(remoteConflictPath, std::ios::binary);
        remoteFile.write(reinterpret_cast<const char*>(remoteData.data()), remoteData.size());
        remoteFile.close();
        logger.info("Saved remote version: " + remoteConflictPath, "ConflictResolver");
        
        // Keep local as-is (user decides which to use)
        return true;
    } catch (const std::exception& e) {
        logger.error("Failed to apply KEEP_BOTH: " + std::string(e.what()), "ConflictResolver");
        return false;
    }
}

bool ConflictResolver::applyLargestWins(
    const FileConflict& conflict,
    const std::string& localPath,
    const std::vector<uint8_t>& remoteData
) {
    auto& logger = Logger::instance();
    try {
        if (conflict.remoteSize > conflict.localSize) {
            // Remote is larger, overwrite local
            std::ofstream file(localPath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(remoteData.data()), remoteData.size());
            file.close();
            logger.info("Applied LARGEST_WINS: Remote version is larger (" + 
                       std::to_string(conflict.remoteSize) + " > " + 
                       std::to_string(conflict.localSize) + " bytes)", "ConflictResolver");
        } else {
            // Local is larger or equal, keep it
            logger.info("Applied LARGEST_WINS: Local version is larger/equal", "ConflictResolver");
        }
        return true;
    } catch (const std::exception& e) {
        logger.error("Failed to apply LARGEST_WINS: " + std::string(e.what()), "ConflictResolver");
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
