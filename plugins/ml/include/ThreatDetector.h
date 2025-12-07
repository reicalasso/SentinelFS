#pragma once

#include "AnomalyDetector.h"
#include "BehaviorProfiler.h"
#include "FileEntropyAnalyzer.h"
#include "PatternMatcher.h"
#include "IsolationForest.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <chrono>
#include <deque>

namespace SentinelFS {

/**
 * @brief Unified threat detection system combining multiple ML techniques
 * 
 * Combines:
 * - Rule-based anomaly detection (rate limits, thresholds)
 * - Behavioral profiling (learns normal patterns)
 * - File entropy analysis (detects encryption)
 * - Pattern matching (known ransomware signatures)
 * - Isolation Forest (unsupervised anomaly detection)
 * 
 * Produces a unified threat score and classification.
 */
class ThreatDetector {
public:
    enum class ThreatType {
        NONE,
        RANSOMWARE,
        DATA_EXFILTRATION,
        MASS_DELETION,
        SUSPICIOUS_ACTIVITY,
        UNKNOWN_ANOMALY
    };

    enum class Severity {
        NONE,
        LOW,
        MEDIUM,
        HIGH,
        CRITICAL
    };

    struct ThreatAlert {
        ThreatType type{ThreatType::NONE};
        Severity severity{Severity::NONE};
        double confidenceScore{0.0};         // 0-1
        std::string description;
        std::string recommendedAction;
        std::vector<std::string> indicators;  // IOCs (Indicators of Compromise)
        std::chrono::system_clock::time_point timestamp;
    };

    struct DetectionStats {
        size_t totalEventsProcessed{0};
        size_t alertsGenerated{0};
        size_t falsePositivesReported{0};
        double avgProcessingTimeMs{0.0};
        std::map<ThreatType, size_t> alertsByType;
    };

    using AlertCallback = std::function<void(const ThreatAlert&)>;

    /**
     * @brief Configuration for threat detection
     */
    struct Config {
        bool enableBehaviorProfiling = true;
        bool enableEntropyAnalysis = true;
        bool enablePatternMatching = true;
        bool enableIsolationForest = true;
        bool enableRuleBasedDetection = true;
        
        double alertThreshold = 0.6;           // Minimum score to generate alert
        int maxAlertsPerMinute = 10;           // Rate limit alerts
        
        // Sensitivity settings (0-1, higher = more sensitive)
        double sensitivityLevel = 0.5;
        
        // Profile persistence
        std::string profilePath;              // Where to save/load profiles
        bool autoSaveProfiles = true;
        int profileSaveIntervalMinutes = 30;
    };

    ThreatDetector();
    explicit ThreatDetector(const Config& config);
    ~ThreatDetector();

    /**
     * @brief Initialize the threat detector
     */
    bool initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Process a file system event
     * @param action Action type (CREATE, MODIFY, DELETE)
     * @param path File path
     * @param fileSize File size (optional)
     * @return Threat alert if detected
     */
    ThreatAlert processEvent(const std::string& action, const std::string& path,
                             size_t fileSize = 0);

    /**
     * @brief Analyze a specific file for threats
     */
    ThreatAlert analyzeFile(const std::string& path);

    /**
     * @brief Get current threat level (overall system state)
     */
    Severity getCurrentThreatLevel() const;

    /**
     * @brief Get current unified threat score
     */
    double getCurrentThreatScore() const;

    /**
     * @brief Set alert callback
     */
    void setAlertCallback(AlertCallback callback);

    /**
     * @brief Report false positive (for learning)
     */
    void reportFalsePositive(const ThreatAlert& alert);

    /**
     * @brief Get detection statistics
     */
    DetectionStats getStats() const;

    /**
     * @brief Get recent alerts
     */
    std::vector<ThreatAlert> getRecentAlerts(size_t maxCount = 100) const;

    /**
     * @brief Save profiles to disk
     */
    bool saveProfiles();

    /**
     * @brief Load profiles from disk
     */
    bool loadProfiles();

    /**
     * @brief Train/update ML models with current data
     */
    void updateModels();

    /**
     * @brief Get component status
     */
    struct ComponentStatus {
        bool behaviorProfilerReady{false};
        double behaviorLearningProgress{0.0};
        bool isolationForestTrained{false};
        size_t patternsLoaded{0};
        size_t entropyBaselinesLoaded{0};
    };
    ComponentStatus getComponentStatus() const;

    // Static helpers
    static std::string threatTypeToString(ThreatType type);
    static std::string severityToString(Severity severity);

private:
    Config config_;
    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};

    // Detection components
    std::unique_ptr<AnomalyDetector> anomalyDetector_;
    std::unique_ptr<BehaviorProfiler> behaviorProfiler_;
    std::unique_ptr<FileEntropyAnalyzer> entropyAnalyzer_;
    std::unique_ptr<PatternMatcher> patternMatcher_;
    std::unique_ptr<IsolationForest> isolationForest_;

    // Alert management
    AlertCallback alertCallback_;
    std::deque<ThreatAlert> recentAlerts_;
    std::chrono::steady_clock::time_point lastAlertTime_;
    int alertsThisMinute_{0};
    static constexpr size_t MAX_RECENT_ALERTS = 1000;

    // Statistics
    mutable std::mutex statsMutex_;
    DetectionStats stats_;

    // Feature collection for ML training
    struct EventRecord {
        std::string action;
        std::string path;
        size_t size;
        double entropy;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::deque<EventRecord> eventHistory_;
    static constexpr size_t MAX_EVENT_HISTORY = 10000;

    // Current threat state
    std::atomic<double> currentThreatScore_{0.0};

    // Helper functions
    ThreatAlert combineDetectionResults(
        double ruleBasedScore,
        const BehaviorProfiler::AnomalyResult& behaviorResult,
        const FileEntropyAnalyzer::EntropyResult& entropyResult,
        const PatternMatcher::PatternMatch& patternResult,
        double isolationForestScore,
        const std::string& path);

    ThreatType classifyThreat(
        const BehaviorProfiler::AnomalyResult& behaviorResult,
        const FileEntropyAnalyzer::EntropyResult& entropyResult,
        const PatternMatcher::PatternMatch& patternResult,
        const std::string& action);

    Severity scoreSeverity(double score) const;
    
    void recordEvent(const std::string& action, const std::string& path,
                     size_t size, double entropy);
    
    void pruneEventHistory();
    bool shouldAlert() const;
    void generateAlert(const ThreatAlert& alert);
};

} // namespace SentinelFS
