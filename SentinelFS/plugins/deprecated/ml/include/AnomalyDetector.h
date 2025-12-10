#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <functional>

namespace SentinelFS {

/**
 * @brief Detects anomalous file activity patterns
 */
class AnomalyDetector {
public:
    using AlertCallback = std::function<void(const std::string& anomalyType, const std::string& details)>;

    struct FileActivity {
        std::string path;
        std::chrono::steady_clock::time_point timestamp;
        std::string action; // CREATE, MODIFY, DELETE
    };

    AnomalyDetector();

    /**
     * @brief Set alert callback
     */
    void setAlertCallback(AlertCallback callback);

    /**
     * @brief Record file activity
     */
    void recordActivity(const std::string& action, const std::string& path);

    /**
     * @brief Analyze recent activity for anomalies
     */
    void analyzeActivity();

    /**
     * @brief Get statistics
     */
    size_t getActivityCount() const { return recentActivity_.size(); }

    /**
     * @brief Get current anomaly score (0.0 = normal, 1.0 = critical)
     */
    double getAnomalyScore() const;

    /**
     * @brief Get last anomaly type (empty if none)
     */
    const std::string& getLastAnomalyType() const { return lastAnomalyType_; }

private:
    std::vector<FileActivity> recentActivity_;
    AlertCallback alertCallback_;
    
    // Detection parameters
    const size_t MAX_ACTIVITY_BUFFER = 1000;
    const int RAPID_MODIFICATION_THRESHOLD = 10; // files per second
    const int RAPID_DELETION_THRESHOLD = 5; // consecutive deletions
    
    // State tracking
    int consecutiveDeletions_ = 0;
    std::chrono::steady_clock::time_point lastCheckTime_;
    
    /**
     * @brief Check for ransomware-like behavior
     */
    void checkForRansomware();

    std::string lastAnomalyType_;
    double currentScore_{0.0};
};

} // namespace SentinelFS
