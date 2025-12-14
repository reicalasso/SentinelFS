#pragma once

/**
 * @file AutoResponse.h
 * @brief Automated threat response system for Zer0
 * 
 * Features:
 * - Automatic threat remediation
 * - Process termination
 * - Network isolation
 * - File quarantine
 * - System rollback
 * - Alert notifications
 */

#include "Zer0Plugin.h"
#include "ProcessMonitor.h"
#include "YaraScanner.h"
#include "MLEngine.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>
#include <queue>

namespace SentinelFS {
namespace Zer0 {

/**
 * @brief Response action types
 */
enum class ResponseAction {
    NONE,
    LOG,                    // Log the event
    ALERT,                  // Send alert notification
    QUARANTINE_FILE,        // Move file to quarantine
    DELETE_FILE,            // Delete the file
    BLOCK_PROCESS,          // Prevent process from running
    TERMINATE_PROCESS,      // Kill the process
    SUSPEND_PROCESS,        // Suspend the process
    ISOLATE_NETWORK,        // Block network access
    BACKUP_FILE,            // Create backup before action
    RESTORE_FILE,           // Restore from backup
    ROLLBACK_CHANGES,       // Rollback recent changes
    CUSTOM                  // Custom action
};

/**
 * @brief Response action result
 */
struct ResponseResult {
    bool success{false};
    ResponseAction action;
    std::string target;
    std::string message;
    std::chrono::steady_clock::time_point timestamp;
    std::map<std::string, std::string> details;
};

/**
 * @brief Response rule
 */
struct ResponseRule {
    std::string id;
    std::string name;
    std::string description;
    
    // Trigger conditions
    ThreatLevel minThreatLevel{ThreatLevel::HIGH};
    std::set<ThreatType> threatTypes;
    std::set<std::string> targetPaths;
    std::set<std::string> targetProcesses;
    double minConfidence{0.8};
    
    // Actions to take
    std::vector<ResponseAction> actions;
    
    // Options
    bool enabled{true};
    bool requireConfirmation{false};
    int priority{0};
    std::chrono::seconds cooldown{60};
    
    // Custom action handler
    std::function<ResponseResult(const DetectionResult&)> customHandler;
};

/**
 * @brief Alert notification
 */
struct Alert {
    std::string id;
    std::string title;
    std::string message;
    ThreatLevel severity;
    std::chrono::steady_clock::time_point timestamp;
    std::string source;
    std::map<std::string, std::string> metadata;
    bool acknowledged{false};
};

/**
 * @brief Alert callback
 */
using AlertCallback = std::function<void(const Alert&)>;

/**
 * @brief Response callback
 */
using ResponseCallback = std::function<void(const ResponseResult&)>;

/**
 * @brief Confirmation callback (returns true to proceed)
 */
using ConfirmationCallback = std::function<bool(const ResponseRule&, const DetectionResult&)>;

/**
 * @brief Auto Response configuration
 */
struct AutoResponseConfig {
    bool enabled{true};
    bool autoQuarantine{true};
    bool autoTerminate{false};          // Dangerous - disabled by default
    bool autoIsolate{false};            // Network isolation
    bool createBackups{true};
    
    // Thresholds
    ThreatLevel quarantineThreshold{ThreatLevel::HIGH};
    ThreatLevel terminateThreshold{ThreatLevel::CRITICAL};
    ThreatLevel alertThreshold{ThreatLevel::MEDIUM};
    
    // Limits
    int maxActionsPerMinute{10};
    int maxQuarantineSize{1024 * 1024 * 1024};  // 1GB
    
    // Paths
    std::string quarantineDir;
    std::string backupDir;
    std::string logDir;
    
    // Whitelist (never act on these)
    std::set<std::string> whitelistedPaths;
    std::set<std::string> whitelistedProcesses;
    std::set<pid_t> whitelistedPids;
};

/**
 * @brief Auto Response system
 */
class AutoResponse {
public:
    AutoResponse();
    ~AutoResponse();
    
    // Non-copyable
    AutoResponse(const AutoResponse&) = delete;
    AutoResponse& operator=(const AutoResponse&) = delete;
    
    /**
     * @brief Initialize the auto response system
     */
    bool initialize(const AutoResponseConfig& config = AutoResponseConfig{});
    
    /**
     * @brief Shutdown the system
     */
    void shutdown();
    
    /**
     * @brief Process a detection result
     */
    std::vector<ResponseResult> processDetection(const DetectionResult& detection);
    
    /**
     * @brief Process a suspicious behavior
     */
    std::vector<ResponseResult> processBehavior(const SuspiciousBehavior& behavior);
    
    /**
     * @brief Process a YARA match
     */
    std::vector<ResponseResult> processYaraMatch(const YaraScanResult& result);
    
    /**
     * @brief Process an ML anomaly
     */
    std::vector<ResponseResult> processAnomaly(const AnomalyResult& anomaly);
    
    // Manual actions
    
    /**
     * @brief Quarantine a file
     */
    ResponseResult quarantineFile(const std::string& path);
    
    /**
     * @brief Restore a file from quarantine
     */
    ResponseResult restoreFile(const std::string& quarantinePath);
    
    /**
     * @brief Delete a file
     */
    ResponseResult deleteFile(const std::string& path);
    
    /**
     * @brief Terminate a process
     */
    ResponseResult terminateProcess(pid_t pid);
    
    /**
     * @brief Suspend a process
     */
    ResponseResult suspendProcess(pid_t pid);
    
    /**
     * @brief Resume a suspended process
     */
    ResponseResult resumeProcess(pid_t pid);
    
    /**
     * @brief Block network for a process
     */
    ResponseResult isolateProcess(pid_t pid);
    
    /**
     * @brief Create backup of a file
     */
    ResponseResult backupFile(const std::string& path);
    
    /**
     * @brief Restore file from backup
     */
    ResponseResult restoreFromBackup(const std::string& path);
    
    /**
     * @brief Rollback recent changes
     */
    ResponseResult rollbackChanges(const std::string& path, 
                                    std::chrono::steady_clock::time_point since);
    
    // Rule management
    
    /**
     * @brief Add a response rule
     */
    void addRule(const ResponseRule& rule);
    
    /**
     * @brief Remove a rule
     */
    void removeRule(const std::string& ruleId);
    
    /**
     * @brief Enable/disable a rule
     */
    void setRuleEnabled(const std::string& ruleId, bool enabled);
    
    /**
     * @brief Get all rules
     */
    std::vector<ResponseRule> getRules() const;
    
    /**
     * @brief Get rule by ID
     */
    ResponseRule getRule(const std::string& ruleId) const;
    
    // Callbacks
    
    /**
     * @brief Set alert callback
     */
    void setAlertCallback(AlertCallback callback);
    
    /**
     * @brief Set response callback
     */
    void setResponseCallback(ResponseCallback callback);
    
    /**
     * @brief Set confirmation callback
     */
    void setConfirmationCallback(ConfirmationCallback callback);
    
    // Alerts
    
    /**
     * @brief Get pending alerts
     */
    std::vector<Alert> getPendingAlerts() const;
    
    /**
     * @brief Acknowledge an alert
     */
    void acknowledgeAlert(const std::string& alertId);
    
    /**
     * @brief Clear all alerts
     */
    void clearAlerts();
    
    // Quarantine management
    
    /**
     * @brief Get quarantined files
     */
    std::vector<std::string> getQuarantinedFiles() const;
    
    /**
     * @brief Get quarantine info for a file
     */
    struct QuarantineInfo {
        std::string originalPath;
        std::string quarantinePath;
        std::chrono::steady_clock::time_point quarantineTime;
        std::string reason;
        ThreatLevel threatLevel;
        std::string hash;
    };
    QuarantineInfo getQuarantineInfo(const std::string& quarantinePath) const;
    
    /**
     * @brief Get total quarantine size
     */
    uint64_t getQuarantineSize() const;
    
    /**
     * @brief Clean old quarantine files
     */
    void cleanQuarantine(std::chrono::hours maxAge = std::chrono::hours(24 * 30));
    
    // Configuration
    
    /**
     * @brief Update configuration
     */
    void setConfig(const AutoResponseConfig& config);
    
    /**
     * @brief Get current configuration
     */
    AutoResponseConfig getConfig() const;
    
    // Statistics
    
    struct Stats {
        uint64_t detectionsProcessed{0};
        uint64_t actionsExecuted{0};
        uint64_t filesQuarantined{0};
        uint64_t filesDeleted{0};
        uint64_t processesTerminated{0};
        uint64_t alertsSent{0};
        uint64_t backupsCreated{0};
        uint64_t rollbacksPerformed{0};
        std::chrono::steady_clock::time_point startTime;
    };
    Stats getStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Default response rules
 */
class DefaultRules {
public:
    /**
     * @brief Get ransomware response rule
     */
    static ResponseRule getRansomwareRule();
    
    /**
     * @brief Get malware response rule
     */
    static ResponseRule getMalwareRule();
    
    /**
     * @brief Get cryptominer response rule
     */
    static ResponseRule getCryptominerRule();
    
    /**
     * @brief Get data exfiltration response rule
     */
    static ResponseRule getExfiltrationRule();
    
    /**
     * @brief Get suspicious process response rule
     */
    static ResponseRule getSuspiciousProcessRule();
    
    /**
     * @brief Get all default rules
     */
    static std::vector<ResponseRule> getAllDefaultRules();
};

} // namespace Zer0
} // namespace SentinelFS
