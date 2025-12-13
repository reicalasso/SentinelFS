#pragma once

/**
 * @file MergeResolver.h
 * @brief 3-way merge algorithm for conflict resolution
 * 
 * Provides intelligent conflict resolution for file content using
 * a three-way merge algorithm with automatic conflict detection
 * and resolution strategies.
 */

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <cstdint>

namespace SentinelFS {

/**
 * @brief Conflict information
 */
struct ConflictInfo {
    size_t startLine;
    size_t endLine;
    std::string localContent;
    std::string remoteContent;
    std::string baseContent;
    std::string description;
};

/**
 * @brief Merge result
 */
struct MergeResult {
    bool success{false};
    std::string mergedContent;
    std::vector<ConflictInfo> conflicts;
    std::string errorMessage;
    size_t conflictsResolved{0};
    size_t autoResolved{0};
};

/**
 * @brief Merge resolution strategy
 */
enum class MergeStrategy {
    LOCAL_WINS,     // Always prefer local version
    REMOTE_WINS,    // Always prefer remote version
    AUTO_MERGE,     // Attempt automatic 3-way merge
    MANUAL,         // Require manual intervention
    TIMESTAMP_WINS, // Prefer most recent modification
    SIZE_WINS       // Prefer larger file
};

/**
 * @brief 3-way merge resolver
 */
class MergeResolver {
public:
    /**
     * @brief Perform 3-way merge
     * @param base Original version before diverging
     * @param local Local modifications
     * @param remote Remote modifications
     * @param strategy Resolution strategy
     * @return Merge result
     */
    static MergeResult merge(
        const std::string& base,
        const std::string& local,
        const std::string& remote,
        MergeStrategy strategy = MergeStrategy::AUTO_MERGE
    );

    /**
     * @brief Merge binary files
     * @param base Original file data
     * @param local Local file data
     * @param remote Remote file data
     * @param strategy Resolution strategy
     * @return Merge result
     */
    static MergeResult mergeBinary(
        const std::vector<uint8_t>& base,
        const std::vector<uint8_t>& local,
        const std::vector<uint8_t>& remote,
        MergeStrategy strategy = MergeStrategy::TIMESTAMP_WINS
    );

    /**
     * @brief Detect conflicts between versions
     * @param base Original version
     * @param local Local version
     * @param remote Remote version
     * @return List of conflicts
     */
    static std::vector<ConflictInfo> detectConflicts(
        const std::string& base,
        const std::string& local,
        const std::string& remote
    );

    /**
     * @brief Attempt automatic conflict resolution
     * @param conflict Conflict to resolve
     * @param strategy Resolution strategy
     * @return Resolved content or empty if failed
     */
    static std::optional<std::string> resolveConflict(
        const ConflictInfo& conflict,
        MergeStrategy strategy
    );

    /**
     * @brief Generate conflict marker for manual resolution
     * @param conflict Conflict information
     * @return Formatted conflict marker
     */
    static std::string generateConflictMarker(const ConflictInfo& conflict);

    /**
     * @brief Parse conflict markers from content
     * @param content Content with conflict markers
     * @return List of conflicts
     */
    static std::vector<ConflictInfo> parseConflictMarkers(const std::string& content);

private:
    // Line-based diff algorithms
    static std::vector<std::string> splitLines(const std::string& content);
    static std::vector<size_t> computeHashes(const std::vector<std::string>& lines);
    static std::vector<size_t> lcs(const std::vector<size_t>& a, const std::vector<size_t>& b);
    
    // Merge helpers
    static MergeResult performThreeWayMerge(
        const std::vector<std::string>& base,
        const std::vector<std::string>& local,
        const std::vector<std::string>& remote
    );
    
    static bool isWhitespaceOnly(const std::string& line);
    static bool isSimilar(const std::string& a, const std::string& b, double threshold = 0.8);
    static std::string normalizeLine(const std::string& line);
};

} // namespace SentinelFS
