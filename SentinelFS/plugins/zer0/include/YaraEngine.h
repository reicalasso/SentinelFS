#pragma once

/**
 * @file YaraEngine.h
 * @brief YARA Engine wrapper for Zer0 threat detection
 * 
 * This is a compatibility wrapper around YaraScanner to maintain
 * the existing Zer0Plugin interface.
 */

#include "YaraScanner.h"
#include <string>
#include <vector>

namespace SentinelFS {
namespace Zer0 {

/**
 * @brief YARA Engine - wrapper around YaraScanner
 */
class YaraEngine {
public:
    YaraEngine() : scanner_() {}
    
    /**
     * @brief Initialize the engine
     */
    bool initialize() {
        return scanner_.initialize();
    }
    
    /**
     * @brief Load rules from file
     */
    bool loadRules(const std::string& path) {
        return scanner_.loadRulesFromFile(path);
    }
    
    /**
     * @brief Update rules from URL (placeholder)
     */
    bool updateRules(const std::string& url) {
        // TODO: Implement URL-based rule updates
        return false;
    }
    
    /**
     * @brief Scan a file
     */
    std::vector<std::string> scanFile(const std::string& path) {
        auto result = scanner_.scanFile(path);
        std::vector<std::string> matches;
        
        for (const auto& match : result.matches) {
            matches.push_back(match.ruleName);
        }
        
        return matches;
    }
    
    /**
     * @brief Get number of loaded rules
     */
    int getRulesCount() const {
        return scanner_.getRuleCount();
    }
    
    /**
     * @brief Get number of files scanned
     */
    uint64_t getFilesScanned() const {
        auto stats = scanner_.getStats();
        return stats.filesScanned;
    }
    
    /**
     * @brief Get number of matches found
     */
    uint64_t getMatchesFound() const {
        auto stats = scanner_.getStats();
        return stats.matchesFound;
    }

private:
    YaraScanner scanner_;
};

} // namespace Zer0
} // namespace SentinelFS
