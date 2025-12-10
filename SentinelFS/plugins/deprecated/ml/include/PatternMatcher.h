#pragma once

#include <string>
#include <vector>
#include <map>
#include <regex>
#include <mutex>
#include <chrono>
#include <set>

namespace SentinelFS {

/**
 * @brief Pattern-based threat detection
 * 
 * Detects known malicious patterns:
 * - Ransomware file extensions (.locked, .encrypted, .crypto, etc.)
 * - Ransom note filenames
 * - Suspicious directory structures
 * - Known malware signatures in filenames
 * - Mass rename patterns (file1.txt -> file1.txt.encrypted)
 */
class PatternMatcher {
public:
    enum class ThreatLevel {
        NONE,
        LOW,
        MEDIUM,
        HIGH,
        CRITICAL
    };

    struct PatternMatch {
        bool matched{false};
        ThreatLevel level{ThreatLevel::NONE};
        std::string patternName;
        std::string description;
        std::string matchedValue;
    };

    struct RansomwarePattern {
        std::string name;
        std::regex pattern;
        ThreatLevel level;
        std::string description;
    };

    PatternMatcher();

    /**
     * @brief Check a filename/path against known malicious patterns
     */
    PatternMatch checkPath(const std::string& path);

    /**
     * @brief Check file content for ransomware indicators
     */
    PatternMatch checkContent(const std::string& path);

    /**
     * @brief Record a file event for pattern analysis
     */
    void recordEvent(const std::string& action, const std::string& path);

    /**
     * @brief Check for mass rename pattern (ransomware indicator)
     */
    PatternMatch checkMassRenamePattern();

    /**
     * @brief Get known ransomware extensions
     */
    static const std::vector<std::string>& getKnownRansomwareExtensions();

    /**
     * @brief Get known ransom note filenames
     */
    static const std::vector<std::string>& getKnownRansomNoteNames();

    /**
     * @brief Add custom pattern
     */
    void addCustomPattern(const std::string& name, const std::string& regexPattern,
                          ThreatLevel level, const std::string& description);

    /**
     * @brief Remove custom pattern
     */
    void removeCustomPattern(const std::string& name);

    /**
     * @brief Get threat level as string
     */
    static std::string threatLevelToString(ThreatLevel level);

    /**
     * @brief Get statistics
     */
    size_t getTotalChecks() const { return totalChecks_; }
    size_t getTotalMatches() const { return totalMatches_; }
    const std::map<std::string, size_t>& getMatchCountByPattern() const { return matchCounts_; }

private:
    mutable std::mutex mutex_;
    
    // Patterns
    std::vector<RansomwarePattern> extensionPatterns_;
    std::vector<RansomwarePattern> filenamePatterns_;
    std::vector<RansomwarePattern> contentPatterns_;
    std::vector<RansomwarePattern> customPatterns_;
    
    // Recent events for pattern analysis
    struct FileEvent {
        std::chrono::steady_clock::time_point timestamp;
        std::string action;
        std::string oldPath;
        std::string newPath;
    };
    std::vector<FileEvent> recentEvents_;
    
    // Tracking renamed files
    std::map<std::string, std::string> renamedFiles_;  // old -> new
    
    // Statistics
    size_t totalChecks_{0};
    size_t totalMatches_{0};
    std::map<std::string, size_t> matchCounts_;
    
    // Configuration
    static constexpr size_t MAX_RECENT_EVENTS = 1000;
    static constexpr int MASS_RENAME_THRESHOLD = 10;  // files in short time
    static constexpr int MASS_RENAME_WINDOW_SECONDS = 60;
    
    // Helper functions
    void initializePatterns();
    std::string extractExtension(const std::string& path) const;
    std::string extractFilename(const std::string& path) const;
    void pruneOldEvents();
};

} // namespace SentinelFS
