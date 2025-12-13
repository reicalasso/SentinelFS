#include "MergeResolver.h"
#include "Logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <openssl/sha.h>

namespace SentinelFS {

MergeResult MergeResolver::merge(
    const std::string& base,
    const std::string& local,
    const std::string& remote,
    MergeStrategy strategy) {
    
    MergeResult result;
    
    try {
        switch (strategy) {
            case MergeStrategy::LOCAL_WINS:
                result.mergedContent = local;
                result.success = true;
                result.autoResolved = 1;
                break;
                
            case MergeStrategy::REMOTE_WINS:
                result.mergedContent = remote;
                result.success = true;
                result.autoResolved = 1;
                break;
                
            case MergeStrategy::TIMESTAMP_WINS:
            case MergeStrategy::SIZE_WINS:
                // These strategies need metadata, default to auto-merge
                result = performThreeWayMerge(
                    splitLines(base),
                    splitLines(local),
                    splitLines(remote)
                );
                break;
                
            case MergeStrategy::AUTO_MERGE:
                result = performThreeWayMerge(
                    splitLines(base),
                    splitLines(local),
                    splitLines(remote)
                );
                break;
                
            case MergeStrategy::MANUAL:
                result.conflicts = detectConflicts(base, local, remote);
                if (result.conflicts.empty()) {
                    result.mergedContent = local; // No conflicts, use local
                    result.success = true;
                } else {
                    // Generate conflict markers for manual resolution
                    std::stringstream ss;
                    ss << local << "\n";
                    for (const auto& conflict : result.conflicts) {
                        ss << generateConflictMarker(conflict) << "\n";
                    }
                    result.mergedContent = ss.str();
                    result.success = false; // Requires manual intervention
                }
                break;
        }
        
        // Calculate statistics
        result.conflictsResolved = result.conflicts.size();
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
        Logger::instance().log(LogLevel::ERROR, 
            "Merge failed: " + std::string(e.what()), 
            "MergeResolver");
    }
    
    return result;
}

MergeResult MergeResolver::mergeBinary(
    const std::vector<uint8_t>& base,
    const std::vector<uint8_t>& local,
    const std::vector<uint8_t>& remote,
    MergeStrategy strategy) {
    
    MergeResult result;
    
    // For binary files, we can't do line-based merging
    // Use simple strategies
    
    if (local == remote) {
        result.mergedContent = std::string(local.begin(), local.end());
        result.success = true;
        result.autoResolved = 1;
        return result;
    }
    
    if (local == base) {
        result.mergedContent = std::string(remote.begin(), remote.end());
        result.success = true;
        result.autoResolved = 1;
        return result;
    }
    
    if (remote == base) {
        result.mergedContent = std::string(local.begin(), local.end());
        result.success = true;
        result.autoResolved = 1;
        return result;
    }
    
    // Conflict detected - use strategy
    switch (strategy) {
        case MergeStrategy::LOCAL_WINS:
            result.mergedContent = std::string(local.begin(), local.end());
            result.success = true;
            break;
            
        case MergeStrategy::REMOTE_WINS:
            result.mergedContent = std::string(remote.begin(), remote.end());
            result.success = true;
            break;
            
        case MergeStrategy::SIZE_WINS:
            if (local.size() >= remote.size()) {
                result.mergedContent = std::string(local.begin(), local.end());
            } else {
                result.mergedContent = std::string(remote.begin(), remote.end());
            }
            result.success = true;
            break;
            
        default:
            result.success = false;
            result.errorMessage = "Binary merge conflict requires manual resolution";
            ConflictInfo conflict;
            conflict.description = "Binary file conflict - cannot auto-merge";
            result.conflicts.push_back(conflict);
            break;
    }
    
    return result;
}

std::vector<ConflictInfo> MergeResolver::detectConflicts(
    const std::string& base,
    const std::string& local,
    const std::string& remote) {
    
    std::vector<ConflictInfo> conflicts;
    
    auto baseLines = splitLines(base);
    auto localLines = splitLines(local);
    auto remoteLines = splitLines(remote);
    
    // Simple conflict detection - find lines that differ
    size_t maxLines = std::max({baseLines.size(), localLines.size(), remoteLines.size()});
    
    for (size_t i = 0; i < maxLines; ++i) {
        std::string baseLine = i < baseLines.size() ? baseLines[i] : "";
        std::string localLine = i < localLines.size() ? localLines[i] : "";
        std::string remoteLine = i < remoteLines.size() ? remoteLines[i] : "";
        
        if (localLine != remoteLine && 
            localLine != baseLine && 
            remoteLine != baseLine) {
            
            ConflictInfo conflict;
            conflict.startLine = i;
            conflict.endLine = i;
            conflict.localContent = localLine;
            conflict.remoteContent = remoteLine;
            conflict.baseContent = baseLine;
            conflict.description = "Line " + std::to_string(i + 1) + " differs";
            
            conflicts.push_back(conflict);
        }
    }
    
    return conflicts;
}

std::optional<std::string> MergeResolver::resolveConflict(
    const ConflictInfo& conflict,
    MergeStrategy strategy) {
    
    switch (strategy) {
        case MergeStrategy::LOCAL_WINS:
            return conflict.localContent;
            
        case MergeStrategy::REMOTE_WINS:
            return conflict.remoteContent;
            
        case MergeStrategy::SIZE_WINS:
            if (conflict.localContent.size() >= conflict.remoteContent.size()) {
                return conflict.localContent;
            } else {
                return conflict.remoteContent;
            }
            
        case MergeStrategy::AUTO_MERGE:
            // Try to auto-resolve based on similarity
            if (isSimilar(conflict.localContent, conflict.remoteContent, 0.9)) {
                // Very similar, use the longer one
                if (conflict.localContent.size() >= conflict.remoteContent.size()) {
                    return conflict.localContent;
                } else {
                    return conflict.remoteContent;
                }
            }
            
            // If one is empty, use the other
            if (conflict.localContent.empty()) return conflict.remoteContent;
            if (conflict.remoteContent.empty()) return conflict.localContent;
            
            // If both are whitespace only, use local
            if (isWhitespaceOnly(conflict.localContent) && isWhitespaceOnly(conflict.remoteContent)) {
                return conflict.localContent;
            }
            
            // Cannot auto-resolve
            return std::nullopt;
            
        default:
            return std::nullopt;
    }
}

std::string MergeResolver::generateConflictMarker(const ConflictInfo& conflict) {
    std::stringstream ss;
    
    ss << "<<<<<<< LOCAL\n";
    ss << conflict.localContent << "\n";
    ss << "=======\n";
    ss << conflict.remoteContent << "\n";
    ss << ">>>>>>> REMOTE\n";
    
    return ss.str();
}

std::vector<ConflictInfo> MergeResolver::parseConflictMarkers(const std::string& content) {
    std::vector<ConflictInfo> conflicts;
    
    auto lines = splitLines(content);
    
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i] == "<<<<<<< LOCAL") {
            ConflictInfo conflict;
            conflict.startLine = i;
            
            // Find the divider
            size_t j = i + 1;
            while (j < lines.size() && lines[j] != "=======") j++;
            if (j >= lines.size()) break;
            
            // Find the end
            size_t k = j + 1;
            while (k < lines.size() && lines[k] != ">>>>>>> REMOTE") k++;
            if (k >= lines.size()) break;
            
            // Extract content
            for (size_t l = i + 1; l < j; ++l) {
                if (!conflict.localContent.empty()) conflict.localContent += "\n";
                conflict.localContent += lines[l];
            }
            
            for (size_t l = j + 1; l < k; ++l) {
                if (!conflict.remoteContent.empty()) conflict.remoteContent += "\n";
                conflict.remoteContent += lines[l];
            }
            
            conflict.endLine = k;
            conflict.description = "Manual merge conflict at line " + std::to_string(i + 1);
            
            conflicts.push_back(conflict);
            i = k;
        }
    }
    
    return conflicts;
}

// Private methods

std::vector<std::string> MergeResolver::splitLines(const std::string& content) {
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string line;
    
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

std::vector<size_t> MergeResolver::computeHashes(const std::vector<std::string>& lines) {
    std::vector<size_t> hashes;
    
    for (const auto& line : lines) {
        size_t hash = 0;
        for (char c : line) {
            hash = hash * 31 + c;
        }
        hashes.push_back(hash);
    }
    
    return hashes;
}

std::vector<size_t> MergeResolver::lcs(const std::vector<size_t>& a, const std::vector<size_t>& b) {
    // Compute longest common subsequence using dynamic programming
    std::vector<std::vector<size_t>> dp(a.size() + 1, std::vector<size_t>(b.size() + 1, 0));
    
    for (size_t i = 1; i <= a.size(); ++i) {
        for (size_t j = 1; j <= b.size(); ++j) {
            if (a[i-1] == b[j-1]) {
                dp[i][j] = dp[i-1][j-1] + 1;
            } else {
                dp[i][j] = std::max(dp[i-1][j], dp[i][j-1]);
            }
        }
    }
    
    // Reconstruct LCS
    std::vector<size_t> lcs;
    size_t i = a.size(), j = b.size();
    
    while (i > 0 && j > 0) {
        if (a[i-1] == b[j-1]) {
            lcs.push_back(a[i-1]);
            --i; --j;
        } else if (dp[i-1][j] > dp[i][j-1]) {
            --i;
        } else {
            --j;
        }
    }
    
    std::reverse(lcs.begin(), lcs.end());
    return lcs;
}

MergeResult MergeResolver::performThreeWayMerge(
    const std::vector<std::string>& base,
    const std::vector<std::string>& local,
    const std::vector<std::string>& remote) {
    
    MergeResult result;
    
    // Compute hashes for LCS
    auto baseHashes = computeHashes(base);
    auto localHashes = computeHashes(local);
    auto remoteHashes = computeHashes(remote);
    
    // Find common subsequences
    auto localCommon = lcs(baseHashes, localHashes);
    auto remoteCommon = lcs(baseHashes, remoteHashes);
    
    // Simple merge strategy: prefer local changes, mark conflicts
    std::vector<std::string> merged;
    size_t baseIdx = 0, localIdx = 0, remoteIdx = 0;
    
    while (baseIdx < base.size() || localIdx < local.size() || remoteIdx < remote.size()) {
        bool baseEnd = baseIdx >= base.size();
        bool localEnd = localIdx >= local.size();
        bool remoteEnd = remoteIdx >= remote.size();
        
        if (!baseEnd && !localEnd && !remoteEnd &&
            base[baseIdx] == local[localIdx] && 
            base[baseIdx] == remote[remoteIdx]) {
            // All three agree
            merged.push_back(base[baseIdx]);
            ++baseIdx; ++localIdx; ++remoteIdx;
        } else if (!localEnd && !remoteEnd && local[localIdx] == remote[remoteIdx]) {
            // Local and remote agree (both changed from base)
            merged.push_back(local[localIdx]);
            ++localIdx; ++remoteIdx;
            if (!baseEnd && base[baseIdx] == local[localIdx-1]) {
                ++baseIdx;
            }
        } else if (!baseEnd && !localEnd && base[baseIdx] == local[localIdx]) {
            // Only local agrees with base (remote changed)
            merged.push_back(local[localIdx]);
            ++baseIdx; ++localIdx;
        } else if (!baseEnd && !remoteEnd && base[baseIdx] == remote[remoteIdx]) {
            // Only remote agrees with base (local changed)
            merged.push_back(remote[remoteIdx]);
            ++baseIdx; ++remoteIdx;
        } else {
            // Conflict!
            ConflictInfo conflict;
            conflict.startLine = merged.size();
            
            // Collect conflicting lines
            std::stringstream localConflict, remoteConflict;
            
            while (localIdx < local.size() && 
                   (baseEnd || baseIdx >= base.size() || local[localIdx] != base[baseIdx])) {
                if (!localConflict.str().empty()) localConflict << "\n";
                localConflict << local[localIdx];
                ++localIdx;
            }
            
            while (remoteIdx < remote.size() && 
                   (baseEnd || baseIdx >= base.size() || remote[remoteIdx] != base[baseIdx])) {
                if (!remoteConflict.str().empty()) remoteConflict << "\n";
                remoteConflict << remote[remoteIdx];
                ++remoteIdx;
            }
            
            conflict.localContent = localConflict.str();
            conflict.remoteContent = remoteConflict.str();
            conflict.baseContent = baseEnd ? "" : base[baseIdx];
            conflict.description = "Merge conflict at line " + std::to_string(conflict.startLine + 1);
            conflict.endLine = merged.size();
            
            result.conflicts.push_back(conflict);
            
            // Add conflict marker
            merged.push_back(generateConflictMarker(conflict));
            
            if (!baseEnd) ++baseIdx;
        }
    }
    
    // Build merged content
    std::stringstream mergedContent;
    for (size_t i = 0; i < merged.size(); ++i) {
        if (i > 0) mergedContent << "\n";
        mergedContent << merged[i];
    }
    
    result.mergedContent = mergedContent.str();
    result.success = result.conflicts.empty();
    
    return result;
}

bool MergeResolver::isWhitespaceOnly(const std::string& line) {
    return line.find_first_not_of(" \t\r\n") == std::string::npos;
}

bool MergeResolver::isSimilar(const std::string& a, const std::string& b, double threshold) {
    if (a.empty() && b.empty()) return true;
    if (a.empty() || b.empty()) return false;
    
    size_t common = 0;
    size_t minLen = std::min(a.size(), b.size());
    
    for (size_t i = 0; i < minLen; ++i) {
        if (a[i] == b[i]) ++common;
    }
    
    double similarity = static_cast<double>(common) / std::max(a.size(), b.size());
    return similarity >= threshold;
}

std::string MergeResolver::normalizeLine(const std::string& line) {
    // Trim whitespace and normalize line endings
    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    
    size_t end = line.find_last_not_of(" \t\r\n");
    return line.substr(start, end - start + 1);
}

} // namespace SentinelFS
