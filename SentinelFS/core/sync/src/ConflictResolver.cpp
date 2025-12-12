#include "ConflictResolver.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <random>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

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
    
    // Validate inputs
    if (localPath.empty()) {
        logger.error("Cannot resolve conflict: local path is empty", "ConflictResolver");
        return false;
    }
    
    if (remoteData.empty() && conflict.strategy != ResolutionStrategy::LOCAL_WINS) {
        logger.warn("Remote data is empty for conflict resolution", "ConflictResolver");
    }
    
    // Check if local file still exists (might have been deleted)
    bool localExists = std::filesystem::exists(localPath);
    if (!localExists && conflict.strategy == ResolutionStrategy::LOCAL_WINS) {
        logger.warn("Local file no longer exists, cannot apply LOCAL_WINS", "ConflictResolver");
        return false;
    }
    
    bool success = false;
    
    try {
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
                if (remoteData.empty()) {
                    logger.error("Cannot apply REMOTE_WINS: remote data is empty", "ConflictResolver");
                    success = false;
                } else {
                    success = writeFileAtomic(localPath, remoteData);
                    if (success) {
                        logger.info("Applied REMOTE_WINS: Overwrote local with remote", "ConflictResolver");
                    } else {
                        logger.error("Failed to write file for REMOTE_WINS", "ConflictResolver");
                    }
                }
                break;
                
            case ResolutionStrategy::LOCAL_WINS:
                // Keep local version, ignore remote
                if (localExists) {
                    success = true;
                    logger.info("Applied LOCAL_WINS: Kept local version", "ConflictResolver");
                } else {
                    logger.error("Cannot apply LOCAL_WINS: local file does not exist", "ConflictResolver");
                    success = false;
                }
                break;
                
            case ResolutionStrategy::MANUAL:
                // Mark for manual resolution (save both versions)
                success = applyKeepBoth(conflict, localPath, remoteData);
                if (success) {
                    logger.warn("MANUAL resolution required. Both versions saved.", "ConflictResolver");
                }
                break;
        }
    } catch (const std::exception& e) {
        logger.error("Exception during conflict resolution: " + std::string(e.what()), "ConflictResolver");
        success = false;
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
            // Remote is newer, overwrite local atomically
            if (remoteData.empty()) {
                logger.error("Cannot apply NEWEST_WINS: remote data is empty", "ConflictResolver");
                return false;
            }
            if (writeFileAtomic(localPath, remoteData)) {
                logger.info("Applied NEWEST_WINS: Remote version is newer", "ConflictResolver");
                return true;
            } else {
                logger.error("Failed to write file for NEWEST_WINS", "ConflictResolver");
                return false;
            }
        } else {
            // Local is newer or equal, keep it
            logger.info("Applied NEWEST_WINS: Local version is newer/equal", "ConflictResolver");
            return true;
        }
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
        
        if (remoteData.empty()) {
            logger.warn("Remote data is empty, creating empty conflict file", "ConflictResolver");
            std::ofstream remoteFile(remoteConflictPath, std::ios::binary);
            remoteFile.close();
        } else {
            if (!writeFileAtomic(remoteConflictPath, remoteData)) {
                logger.error("Failed to save remote conflict version", "ConflictResolver");
                return false;
            }
        }
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
            // Remote is larger, overwrite local atomically
            if (remoteData.empty()) {
                logger.error("Cannot apply LARGEST_WINS: remote data is empty", "ConflictResolver");
                return false;
            }
            if (writeFileAtomic(localPath, remoteData)) {
                logger.info("Applied LARGEST_WINS: Remote version is larger (" + 
                           std::to_string(conflict.remoteSize) + " > " + 
                           std::to_string(conflict.localSize) + " bytes)", "ConflictResolver");
                return true;
            } else {
                logger.error("Failed to write file for LARGEST_WINS", "ConflictResolver");
                return false;
            }
        } else {
            // Local is larger or equal, keep it
            logger.info("Applied LARGEST_WINS: Local version is larger/equal", "ConflictResolver");
            return true;
        }
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

bool ConflictResolver::writeFileAtomic(
    const std::string& path,
    const std::vector<uint8_t>& data
) {
    namespace fs = std::filesystem;
    auto& logger = Logger::instance();
    
    try {
        // Generate temp file name in same directory
        fs::path targetPath(path);
        fs::path dir = targetPath.parent_path();
        std::string filename = targetPath.filename().string();
        
        // Create random suffix
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 999999);
        std::string tempName = "." + filename + "." + std::to_string(dis(gen)) + ".tmp";
        fs::path tempPath = dir / tempName;
        
        // Ensure parent directory exists
        if (!dir.empty() && !fs::exists(dir)) {
            fs::create_directories(dir);
        }
        
        // Write to temp file
        int fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            logger.error("Failed to create temp file: " + tempPath.string(), "ConflictResolver");
            return false;
        }
        
        ssize_t written = write(fd, data.data(), data.size());
        if (written < 0 || static_cast<size_t>(written) != data.size()) {
            close(fd);
            unlink(tempPath.c_str());
            logger.error("Failed to write all data to temp file", "ConflictResolver");
            return false;
        }
        
        // Sync to disk
        if (fsync(fd) < 0) {
            close(fd);
            unlink(tempPath.c_str());
            logger.error("Failed to sync temp file to disk", "ConflictResolver");
            return false;
        }
        
        close(fd);
        
        // Atomic rename
        if (rename(tempPath.c_str(), path.c_str()) < 0) {
            unlink(tempPath.c_str());
            logger.error("Failed to atomically rename temp file", "ConflictResolver");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        logger.error("Exception in writeFileAtomic: " + std::string(e.what()), "ConflictResolver");
        return false;
    }
}

} // namespace SentinelFS
