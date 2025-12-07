#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <cmath>
#include <deque>

namespace SentinelFS {

/**
 * @brief User/System behavior profiling for anomaly detection
 * 
 * Learns normal behavior patterns and detects deviations:
 * - Activity rate per hour of day
 * - File type distribution
 * - Directory access patterns
 * - Session duration patterns
 */
class BehaviorProfiler {
public:
    struct HourlyProfile {
        double meanActivityRate{0.0};      // Average activities per minute
        double stdDevActivityRate{0.0};     // Standard deviation
        int sampleCount{0};
    };

    struct DirectoryProfile {
        int accessCount{0};
        double normalFrequency{0.0};        // Expected access frequency (0-1)
        std::chrono::steady_clock::time_point lastAccess;
    };

    struct FileTypeProfile {
        int createCount{0};
        int modifyCount{0};
        int deleteCount{0};
        double normalRatio{0.0};            // Expected ratio of this file type
    };

    struct AnomalyResult {
        bool isAnomaly{false};
        double score{0.0};                  // 0.0 = normal, 1.0 = highly anomalous
        std::string reason;
        std::string category;               // "RATE", "PATTERN", "TIME", "DIRECTORY"
    };

    BehaviorProfiler();

    /**
     * @brief Record an activity for profiling
     */
    void recordActivity(const std::string& action, const std::string& path);

    /**
     * @brief Check if current behavior is anomalous
     */
    AnomalyResult checkForAnomaly();

    /**
     * @brief Get current activity rate (per minute)
     */
    double getCurrentActivityRate() const;

    /**
     * @brief Get profile learning progress (0-1)
     */
    double getLearningProgress() const;

    /**
     * @brief Check if profile has enough data for detection
     */
    bool isProfileReady() const;

    /**
     * @brief Save profile to file
     */
    bool saveProfile(const std::string& path) const;

    /**
     * @brief Load profile from file
     */
    bool loadProfile(const std::string& path);

    /**
     * @brief Reset profile (start learning from scratch)
     */
    void reset();

    /**
     * @brief Get statistics
     */
    size_t getTotalActivities() const { return totalActivities_; }
    size_t getProfiledHours() const;
    size_t getProfiledDirectories() const { return directoryProfiles_.size(); }

private:
    mutable std::mutex mutex_;
    
    // Time-based profiling
    std::map<int, HourlyProfile> hourlyProfiles_;  // Hour (0-23) -> Profile
    
    // Directory profiling
    std::map<std::string, DirectoryProfile> directoryProfiles_;
    
    // File type profiling (by extension)
    std::map<std::string, FileTypeProfile> fileTypeProfiles_;
    
    // Recent activity tracking
    struct TimestampedActivity {
        std::chrono::steady_clock::time_point timestamp;
        std::string action;
        std::string path;
    };
    std::deque<TimestampedActivity> recentActivities_;
    
    // Configuration
    static constexpr size_t MAX_RECENT_ACTIVITIES = 1000;
    static constexpr size_t MIN_SAMPLES_FOR_PROFILE = 100;
    static constexpr double ANOMALY_THRESHOLD_SIGMA = 3.0;  // 3 standard deviations
    static constexpr int ACTIVITY_WINDOW_SECONDS = 60;
    
    // Statistics
    size_t totalActivities_{0};
    std::chrono::steady_clock::time_point profileStartTime_;
    
    // Helper functions
    void updateHourlyProfile(int hour, double rate);
    void updateDirectoryProfile(const std::string& directory);
    void updateFileTypeProfile(const std::string& extension, const std::string& action);
    std::string extractExtension(const std::string& path) const;
    std::string extractDirectory(const std::string& path) const;
    double calculateZScore(double value, double mean, double stdDev) const;
    void pruneOldActivities();
};

} // namespace SentinelFS
