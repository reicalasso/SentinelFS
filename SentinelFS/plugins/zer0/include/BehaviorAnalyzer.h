#pragma once

/**
 * @file BehaviorAnalyzer.h
 * @brief Behavioral analysis for ransomware and anomaly detection
 */

#include "Zer0Plugin.h"
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace SentinelFS {
namespace Zer0 {

/**
 * @brief Behavioral pattern types
 */
enum class BehaviorPattern {
    NORMAL,
    MASS_CREATION,          // Many files created quickly
    MASS_MODIFICATION,      // Many files modified quickly
    MASS_DELETION,          // Many files deleted quickly
    MASS_RENAME,            // Many files renamed quickly
    EXTENSION_CHANGE,       // File extensions being changed
    ENCRYPTION_PATTERN,     // Files being encrypted (entropy increase)
    SEQUENTIAL_ACCESS,      // Files accessed in sequence (backup/copy)
    RANDOM_ACCESS,          // Random file access (normal usage)
    SINGLE_PROCESS_STORM    // One process modifying many files
};

/**
 * @brief Behavioral analysis result
 */
struct BehaviorAnalysis {
    BehaviorPattern pattern{BehaviorPattern::NORMAL};
    ThreatLevel threatLevel{ThreatLevel::NONE};
    double confidence{0.0};
    std::string description;
    
    // Statistics
    int eventsInWindow{0};
    int uniqueFiles{0};
    int uniqueProcesses{0};
    
    // Suspicious process (if any)
    pid_t suspiciousPid{0};
    std::string suspiciousProcess;
    int processEventCount{0};
};

/**
 * @brief Behavioral analyzer
 */
class BehaviorAnalyzer {
public:
    BehaviorAnalyzer();
    ~BehaviorAnalyzer();
    
    /**
     * @brief Start the analyzer
     */
    void start(std::chrono::seconds windowSize = std::chrono::seconds(60));
    
    /**
     * @brief Stop the analyzer
     */
    void stop();
    
    /**
     * @brief Record an event
     */
    void recordEvent(const BehaviorEvent& event);
    
    /**
     * @brief Analyze current behavior
     */
    BehaviorAnalysis analyze();
    
    /**
     * @brief Set thresholds
     */
    void setMassModificationThreshold(int threshold);
    void setMassRenameThreshold(int threshold);
    void setMassDeletionThreshold(int threshold);
    
    /**
     * @brief Check for ransomware patterns
     */
    bool detectRansomwarePattern();
    
    /**
     * @brief Get events for a specific process
     */
    std::vector<BehaviorEvent> getProcessEvents(pid_t pid) const;
    
    /**
     * @brief Get recent events
     */
    std::vector<BehaviorEvent> getRecentEvents(std::chrono::seconds duration) const;
    
    /**
     * @brief Clear old events
     */
    void pruneOldEvents();
    
    /**
     * @brief Statistics
     */
    struct Stats {
        uint64_t totalEvents{0};
        uint64_t anomaliesDetected{0};
        uint64_t ransomwarePatternsDetected{0};
    };
    Stats getStats() const;

private:
    std::deque<BehaviorEvent> events_;
    mutable std::mutex mutex_;
    
    std::chrono::seconds windowSize_{60};
    int massModificationThreshold_{50};
    int massRenameThreshold_{10};
    int massDeletionThreshold_{20};
    
    std::atomic<bool> running_{false};
    std::thread pruneThread_;
    std::condition_variable cv_;
    
    Stats stats_;
    
    void pruneLoop();
    
    // Analysis helpers
    int countEventType(const std::string& type) const;
    int countUniqueFiles() const;
    int countUniqueProcesses() const;
    std::map<pid_t, int> getProcessEventCounts() const;
    bool detectExtensionChangePattern() const;
};

} // namespace Zer0
} // namespace SentinelFS
