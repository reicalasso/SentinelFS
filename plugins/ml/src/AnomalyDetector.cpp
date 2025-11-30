#include "AnomalyDetector.h"
#include <iostream>

namespace SentinelFS {

AnomalyDetector::AnomalyDetector() 
    : lastCheckTime_(std::chrono::steady_clock::now()) {}

void AnomalyDetector::setAlertCallback(AlertCallback callback) {
    alertCallback_ = callback;
}

void AnomalyDetector::recordActivity(const std::string& action, const std::string& path) {
    FileActivity activity;
    activity.path = path;
    activity.action = action;
    activity.timestamp = std::chrono::steady_clock::now();
    
    recentActivity_.push_back(activity);
    
    // Keep buffer size limited
    if (recentActivity_.size() > MAX_ACTIVITY_BUFFER) {
        recentActivity_.erase(recentActivity_.begin());
    }
    
    // Track consecutive deletions
    if (action == "DELETE") {
        consecutiveDeletions_++;
        checkForRansomware();
    } else {
        consecutiveDeletions_ = 0;
    }
    
    // Analyze for anomalies
    analyzeActivity();
}

void AnomalyDetector::analyzeActivity() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastCheckTime_
    ).count();
    
    if (timeSinceLastCheck < 1) return; // Check every second
    
    // Count recent modifications (last 1 second)
    int recentMods = 0;
    for (const auto& activity : recentActivity_) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - activity.timestamp
        ).count();
        
        if (age <= 1 && activity.action == "MODIFY") {
            recentMods++;
        }
    }
    
    // Check for rapid modifications (potential ransomware)
    if (recentMods >= RAPID_MODIFICATION_THRESHOLD) {
        std::cout << "⚠️  ML ALERT: Rapid file modifications detected (" 
                  << recentMods << " files/sec) - Possible ransomware!" << std::endl;
        
        lastAnomalyType_ = "RAPID_MODIFICATIONS";
        // Score based on how far above threshold
        double ratio = static_cast<double>(recentMods) / RAPID_MODIFICATION_THRESHOLD;
        currentScore_ = std::min(1.0, ratio * 0.5);
        
        if (alertCallback_) {
            alertCallback_("RAPID_MODIFICATIONS", 
                          "Detected " + std::to_string(recentMods) + " modifications/sec");
        }
    }
    
    lastCheckTime_ = now;
}

void AnomalyDetector::checkForRansomware() {
    if (consecutiveDeletions_ >= RAPID_DELETION_THRESHOLD) {
        std::cout << "⚠️  ML ALERT: Multiple consecutive deletions (" 
                  << consecutiveDeletions_ << ") - Possible data destruction!" << std::endl;
        
        lastAnomalyType_ = "RAPID_DELETIONS";
        // Score based on how far above threshold
        double ratio = static_cast<double>(consecutiveDeletions_) / RAPID_DELETION_THRESHOLD;
        currentScore_ = std::min(1.0, ratio * 0.5);
        
        if (alertCallback_) {
            alertCallback_("RAPID_DELETIONS", 
                          std::to_string(consecutiveDeletions_) + " consecutive deletions");
        }
    }
}

double AnomalyDetector::getAnomalyScore() const {
    // Decay score over time (simple linear decay)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheckTime_).count();
    if (elapsed > 60) {
        return 0.0; // Score decays to 0 after 60 seconds of no activity
    }
    double decay = 1.0 - (static_cast<double>(elapsed) / 60.0);
    return currentScore_ * decay;
}

} // namespace SentinelFS
