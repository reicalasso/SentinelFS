#pragma once

/**
 * @file ProcessMonitor.h
 * @brief Real-time process monitoring for Zer0 threat detection
 * 
 * Features:
 * - Process creation/termination tracking
 * - Process tree analysis (parent-child relationships)
 * - Command line and environment monitoring
 * - Resource usage tracking
 * - Suspicious behavior detection
 */

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>

namespace SentinelFS {
namespace Zer0 {

/**
 * @brief Process information
 */
struct ProcessInfo {
    pid_t pid{0};
    pid_t ppid{0};                          // Parent PID
    std::string name;
    std::string path;                        // Full executable path
    std::string cmdline;                     // Command line arguments
    std::string cwd;                         // Current working directory
    uid_t uid{0};
    gid_t gid{0};
    std::string username;
    
    // Timing
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastSeen;
    
    // Resource usage
    double cpuPercent{0.0};
    uint64_t memoryBytes{0};
    uint64_t virtualMemoryBytes{0};
    uint64_t readBytes{0};
    uint64_t writeBytes{0};
    
    // Network
    int openConnections{0};
    int listeningPorts{0};
    
    // File access
    std::vector<std::string> openFiles;
    int fileDescriptorCount{0};
    
    // Children
    std::vector<pid_t> children;
    
    // Flags
    bool isElevated{false};                  // Running as root/admin
    bool isHidden{false};                    // Attempting to hide
    bool isSuspicious{false};
};

/**
 * @brief Process event types
 */
enum class ProcessEventType {
    CREATED,
    TERMINATED,
    EXEC,                                    // Process executed new binary
    FORK,                                    // Process forked
    PRIVILEGE_CHANGE,                        // UID/GID changed
    FILE_ACCESS,                             // File opened/closed
    NETWORK_CONNECT,                         // Network connection made
    NETWORK_LISTEN,                          // Started listening on port
    SUSPICIOUS_ACTIVITY
};

/**
 * @brief Process event
 */
struct ProcessEvent {
    ProcessEventType type;
    ProcessInfo process;
    std::chrono::steady_clock::time_point timestamp;
    std::string details;
    std::map<std::string, std::string> metadata;
};

/**
 * @brief Suspicious behavior indicators
 */
struct SuspiciousBehavior {
    std::string type;
    std::string description;
    double severity{0.0};                    // 0.0 - 1.0
    ProcessInfo process;
    std::vector<std::string> indicators;
};

/**
 * @brief Process event callback
 */
using ProcessEventCallback = std::function<void(const ProcessEvent&)>;

/**
 * @brief Suspicious behavior callback
 */
using SuspiciousBehaviorCallback = std::function<void(const SuspiciousBehavior&)>;

/**
 * @brief Process Monitor configuration
 */
struct ProcessMonitorConfig {
    bool trackFileAccess{true};
    bool trackNetworkActivity{true};
    bool trackResourceUsage{true};
    bool detectHiddenProcesses{true};
    bool detectPrivilegeEscalation{true};
    
    // Thresholds
    double cpuThreshold{90.0};               // Alert if CPU > 90%
    uint64_t memoryThreshold{1024 * 1024 * 1024};  // Alert if memory > 1GB
    int maxFileDescriptors{1000};
    int maxChildren{100};
    
    // Whitelist
    std::set<std::string> whitelistedProcesses;
    std::set<std::string> whitelistedPaths;
    std::set<pid_t> whitelistedPids;
    
    // Update interval
    std::chrono::milliseconds pollInterval{1000};
};

/**
 * @brief Process Monitor class
 */
class ProcessMonitor {
public:
    ProcessMonitor();
    ~ProcessMonitor();
    
    // Non-copyable
    ProcessMonitor(const ProcessMonitor&) = delete;
    ProcessMonitor& operator=(const ProcessMonitor&) = delete;
    
    /**
     * @brief Initialize the monitor
     */
    bool initialize(const ProcessMonitorConfig& config = ProcessMonitorConfig{});
    
    /**
     * @brief Start monitoring
     */
    bool start();
    
    /**
     * @brief Stop monitoring
     */
    void stop();
    
    /**
     * @brief Check if monitoring is active
     */
    bool isRunning() const;
    
    /**
     * @brief Get process information
     */
    ProcessInfo getProcessInfo(pid_t pid) const;
    
    /**
     * @brief Get all tracked processes
     */
    std::vector<ProcessInfo> getAllProcesses() const;
    
    /**
     * @brief Get process tree (parent and all descendants)
     */
    std::vector<ProcessInfo> getProcessTree(pid_t rootPid) const;
    
    /**
     * @brief Get children of a process
     */
    std::vector<ProcessInfo> getChildren(pid_t pid) const;
    
    /**
     * @brief Get parent chain (ancestors)
     */
    std::vector<ProcessInfo> getParentChain(pid_t pid) const;
    
    /**
     * @brief Find processes by name
     */
    std::vector<ProcessInfo> findByName(const std::string& name) const;
    
    /**
     * @brief Find processes by path
     */
    std::vector<ProcessInfo> findByPath(const std::string& path) const;
    
    /**
     * @brief Check if process is suspicious
     */
    bool isSuspicious(pid_t pid) const;
    
    /**
     * @brief Get suspicious behaviors for a process
     */
    std::vector<SuspiciousBehavior> getSuspiciousBehaviors(pid_t pid) const;
    
    /**
     * @brief Set event callback
     */
    void setEventCallback(ProcessEventCallback callback);
    
    /**
     * @brief Set suspicious behavior callback
     */
    void setSuspiciousBehaviorCallback(SuspiciousBehaviorCallback callback);
    
    /**
     * @brief Add process to whitelist
     */
    void whitelistProcess(const std::string& name);
    void whitelistPath(const std::string& path);
    void whitelistPid(pid_t pid);
    
    /**
     * @brief Remove from whitelist
     */
    void removeFromWhitelist(const std::string& name);
    void removeFromWhitelist(pid_t pid);
    
    /**
     * @brief Update configuration
     */
    void setConfig(const ProcessMonitorConfig& config);
    ProcessMonitorConfig getConfig() const;
    
    /**
     * @brief Statistics
     */
    struct Stats {
        uint64_t processesTracked{0};
        uint64_t eventsProcessed{0};
        uint64_t suspiciousDetected{0};
        uint64_t processCreations{0};
        uint64_t processTerminations{0};
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    Stats getStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Process tree analyzer
 */
class ProcessTreeAnalyzer {
public:
    /**
     * @brief Analyze process tree for suspicious patterns
     */
    static std::vector<SuspiciousBehavior> analyzeTree(const std::vector<ProcessInfo>& tree);
    
    /**
     * @brief Detect process injection
     */
    static bool detectInjection(const ProcessInfo& parent, const ProcessInfo& child);
    
    /**
     * @brief Detect process hollowing
     */
    static bool detectHollowing(const ProcessInfo& process);
    
    /**
     * @brief Detect privilege escalation chain
     */
    static bool detectPrivilegeEscalation(const std::vector<ProcessInfo>& chain);
    
    /**
     * @brief Calculate process risk score
     */
    static double calculateRiskScore(const ProcessInfo& process);
    
    /**
     * @brief Get common attack patterns
     */
    static std::vector<std::string> getAttackPatterns(const ProcessInfo& process);
};

/**
 * @brief Known malicious process patterns
 */
class MaliciousPatterns {
public:
    /**
     * @brief Check if process name matches known malware
     */
    static bool isKnownMalware(const std::string& name);
    
    /**
     * @brief Check if command line is suspicious
     */
    static bool isSuspiciousCommandLine(const std::string& cmdline);
    
    /**
     * @brief Check if path is suspicious
     */
    static bool isSuspiciousPath(const std::string& path);
    
    /**
     * @brief Get suspicion reasons
     */
    static std::vector<std::string> getSuspicionReasons(const ProcessInfo& process);
    
    /**
     * @brief Common malware process names
     */
    static const std::vector<std::string>& getKnownMalwareNames();
    
    /**
     * @brief Suspicious command patterns
     */
    static const std::vector<std::string>& getSuspiciousCommandPatterns();
};

} // namespace Zer0
} // namespace SentinelFS
