#pragma once

/**
 * @file YaraScanner.h
 * @brief YARA rule scanner for Zer0 threat detection
 * 
 * Features:
 * - Load and compile YARA rules
 * - Scan files and memory buffers
 * - Rule management (add, remove, reload)
 * - Match result handling
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>

namespace SentinelFS {
namespace Zer0 {

/**
 * @brief YARA match metadata
 */
struct YaraMatch {
    std::string ruleName;
    std::string ruleDescription;
    std::string category;
    std::string severity;          // low, medium, high, critical
    std::vector<std::string> tags;
    std::map<std::string, std::string> metadata;
    
    // Match details
    std::vector<std::pair<size_t, size_t>> matchOffsets;  // offset, length pairs
    std::vector<std::string> matchedStrings;
};

/**
 * @brief Scan result
 */
struct YaraScanResult {
    bool success{false};
    std::string filePath;
    std::vector<YaraMatch> matches;
    std::string error;
    double scanTimeMs{0.0};
    size_t bytesScanned{0};
    
    bool hasMatches() const { return !matches.empty(); }
    bool hasCritical() const {
        for (const auto& m : matches) {
            if (m.severity == "critical") return true;
        }
        return false;
    }
    bool hasHigh() const {
        for (const auto& m : matches) {
            if (m.severity == "high" || m.severity == "critical") return true;
        }
        return false;
    }
};

/**
 * @brief Callback for scan progress
 */
using ScanProgressCallback = std::function<void(const std::string& file, int percent)>;

/**
 * @brief Callback for match notification
 */
using MatchCallback = std::function<void(const YaraMatch& match)>;

/**
 * @brief YARA Scanner class
 */
class YaraScanner {
public:
    YaraScanner();
    ~YaraScanner();
    
    // Non-copyable
    YaraScanner(const YaraScanner&) = delete;
    YaraScanner& operator=(const YaraScanner&) = delete;
    
    /**
     * @brief Initialize the scanner
     */
    bool initialize();
    
    /**
     * @brief Shutdown the scanner
     */
    void shutdown();
    
    /**
     * @brief Load rules from file
     */
    bool loadRulesFromFile(const std::string& path);
    
    /**
     * @brief Load rules from string
     */
    bool loadRulesFromString(const std::string& rules, const std::string& namespace_ = "default");
    
    /**
     * @brief Load rules from directory
     */
    bool loadRulesFromDirectory(const std::string& dirPath);
    
    /**
     * @brief Compile all loaded rules
     */
    bool compileRules();
    
    /**
     * @brief Scan a file
     */
    YaraScanResult scanFile(const std::string& path);
    
    /**
     * @brief Scan memory buffer
     */
    YaraScanResult scanBuffer(const std::vector<uint8_t>& buffer, const std::string& identifier = "");
    
    /**
     * @brief Scan a process memory
     */
    YaraScanResult scanProcess(pid_t pid);
    
    /**
     * @brief Scan directory recursively
     */
    std::vector<YaraScanResult> scanDirectory(const std::string& dirPath, 
                                               bool recursive = true,
                                               ScanProgressCallback progressCallback = nullptr);
    
    /**
     * @brief Set match callback
     */
    void setMatchCallback(MatchCallback callback);
    
    /**
     * @brief Set scan timeout (milliseconds)
     */
    void setTimeout(int timeoutMs);
    
    /**
     * @brief Get loaded rule count
     */
    int getRuleCount() const;
    
    /**
     * @brief Get rule names
     */
    std::vector<std::string> getRuleNames() const;
    
    /**
     * @brief Check if a specific rule is loaded
     */
    bool hasRule(const std::string& ruleName) const;
    
    /**
     * @brief Clear all rules
     */
    void clearRules();
    
    /**
     * @brief Get last error message
     */
    std::string getLastError() const;
    
    /**
     * @brief Statistics
     */
    struct Stats {
        uint64_t filesScanned{0};
        uint64_t bytesScanned{0};
        uint64_t matchesFound{0};
        uint64_t scanErrors{0};
        double totalScanTimeMs{0.0};
    };
    Stats getStats() const;
    void resetStats();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Rule builder helper
 */
class YaraRuleBuilder {
public:
    YaraRuleBuilder(const std::string& ruleName);
    
    YaraRuleBuilder& setDescription(const std::string& desc);
    YaraRuleBuilder& setAuthor(const std::string& author);
    YaraRuleBuilder& setSeverity(const std::string& severity);
    YaraRuleBuilder& setCategory(const std::string& category);
    YaraRuleBuilder& addTag(const std::string& tag);
    YaraRuleBuilder& addMeta(const std::string& key, const std::string& value);
    YaraRuleBuilder& addStringPattern(const std::string& name, const std::string& pattern, 
                                       bool wide = false, bool nocase = false, bool ascii = true);
    YaraRuleBuilder& addHexPattern(const std::string& name, const std::string& hexPattern);
    YaraRuleBuilder& addRegexPattern(const std::string& name, const std::string& regex);
    YaraRuleBuilder& setCondition(const std::string& condition);
    
    std::string build() const;
    
private:
    std::string name_;
    std::map<std::string, std::string> meta_;
    std::vector<std::string> tags_;
    std::vector<std::string> strings_;
    std::string condition_;
};

} // namespace Zer0
} // namespace SentinelFS
