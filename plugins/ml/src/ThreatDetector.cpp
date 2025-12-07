#include "ThreatDetector.h"
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace SentinelFS {

ThreatDetector::ThreatDetector()
    : config_(Config{})
    , lastAlertTime_(std::chrono::steady_clock::now()) {
}

ThreatDetector::ThreatDetector(const Config& config)
    : config_(config)
    , lastAlertTime_(std::chrono::steady_clock::now()) {
}

ThreatDetector::~ThreatDetector() {
    shutdown();
}

bool ThreatDetector::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) return true;
    
    // Create detection components
    if (config_.enableRuleBasedDetection) {
        anomalyDetector_ = std::make_unique<AnomalyDetector>();
    }
    
    if (config_.enableBehaviorProfiling) {
        behaviorProfiler_ = std::make_unique<BehaviorProfiler>();
    }
    
    if (config_.enableEntropyAnalysis) {
        entropyAnalyzer_ = std::make_unique<FileEntropyAnalyzer>();
    }
    
    if (config_.enablePatternMatching) {
        patternMatcher_ = std::make_unique<PatternMatcher>();
    }
    
    if (config_.enableIsolationForest) {
        IsolationForest::Config iforestConfig;
        iforestConfig.numTrees = 100;
        iforestConfig.sampleSize = 256;
        iforestConfig.contaminationRate = 0.1;
        isolationForest_ = std::make_unique<IsolationForest>(iforestConfig);
    }
    
    // Load saved profiles
    if (!config_.profilePath.empty()) {
        loadProfiles();
    }
    
    running_ = true;
    return true;
}

void ThreatDetector::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) return;
    
    // Save profiles before shutdown
    if (config_.autoSaveProfiles && !config_.profilePath.empty()) {
        saveProfiles();
    }
    
    running_ = false;
    
    anomalyDetector_.reset();
    behaviorProfiler_.reset();
    entropyAnalyzer_.reset();
    patternMatcher_.reset();
    isolationForest_.reset();
}

ThreatDetector::ThreatAlert ThreatDetector::processEvent(
    const std::string& action, const std::string& path, size_t fileSize) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto startTime = std::chrono::steady_clock::now();
    ThreatAlert alert;
    alert.timestamp = std::chrono::system_clock::now();
    
    double ruleBasedScore = 0.0;
    BehaviorProfiler::AnomalyResult behaviorResult;
    FileEntropyAnalyzer::EntropyResult entropyResult;
    PatternMatcher::PatternMatch patternResult;
    double iforestScore = 0.0;
    double fileEntropy = 0.0;
    
    // 1. Rule-based detection
    if (anomalyDetector_) {
        anomalyDetector_->recordActivity(action, path);
        ruleBasedScore = anomalyDetector_->getAnomalyScore();
    }
    
    // 2. Behavior profiling
    if (behaviorProfiler_) {
        behaviorProfiler_->recordActivity(action, path);
        behaviorResult = behaviorProfiler_->checkForAnomaly();
    }
    
    // 3. Pattern matching
    if (patternMatcher_) {
        patternResult = patternMatcher_->checkPath(path);
        patternMatcher_->recordEvent(action, path);
        
        // Also check for mass rename pattern
        if (!patternResult.matched) {
            patternResult = patternMatcher_->checkMassRenamePattern();
        }
    }
    
    // 4. Entropy analysis (only for existing files on CREATE/MODIFY)
    if (entropyAnalyzer_ && (action == "CREATE" || action == "MODIFY")) {
        if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
            entropyResult = entropyAnalyzer_->analyzeFile(path);
            fileEntropy = entropyResult.entropy;
        }
    }
    
    // 5. Isolation Forest
    if (isolationForest_ && isolationForest_->isTrained()) {
        // Extract features from recent history
        std::vector<std::tuple<std::string, std::string, size_t, double>> recentEvents;
        auto now = std::chrono::steady_clock::now();
        
        for (const auto& event : eventHistory_) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - event.timestamp).count();
            if (age <= 60) {  // Last minute
                recentEvents.emplace_back(event.action, event.path, event.size, event.entropy);
            }
        }
        
        if (!recentEvents.empty()) {
            auto features = FeatureExtractor::extractFeatures(recentEvents);
            iforestScore = isolationForest_->predict(features.toVector());
        }
    }
    
    // Record event for history
    recordEvent(action, path, fileSize, fileEntropy);
    
    // Combine results
    alert = combineDetectionResults(ruleBasedScore, behaviorResult, entropyResult,
                                     patternResult, iforestScore, path);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.totalEventsProcessed++;
        
        auto endTime = std::chrono::steady_clock::now();
        double processingTime = std::chrono::duration<double, std::milli>(
            endTime - startTime).count();
        
        // Running average
        stats_.avgProcessingTimeMs = (stats_.avgProcessingTimeMs * 
            (stats_.totalEventsProcessed - 1) + processingTime) / stats_.totalEventsProcessed;
    }
    
    // Generate alert if threshold exceeded
    if (alert.severity != Severity::NONE && 
        alert.confidenceScore >= config_.alertThreshold) {
        generateAlert(alert);
    }
    
    // Update current threat score (exponential moving average)
    double newScore = alert.confidenceScore;
    currentThreatScore_ = currentThreatScore_ * 0.9 + newScore * 0.1;
    
    return alert;
}

ThreatDetector::ThreatAlert ThreatDetector::analyzeFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ThreatAlert alert;
    alert.timestamp = std::chrono::system_clock::now();
    
    if (!std::filesystem::exists(path)) {
        return alert;
    }
    
    double maxScore = 0.0;
    
    // Check patterns
    if (patternMatcher_) {
        auto patternResult = patternMatcher_->checkPath(path);
        if (patternResult.matched) {
            alert.indicators.push_back("Pattern: " + patternResult.patternName);
            
            double patternScore = 0.0;
            switch (patternResult.level) {
                case PatternMatcher::ThreatLevel::LOW: patternScore = 0.3; break;
                case PatternMatcher::ThreatLevel::MEDIUM: patternScore = 0.5; break;
                case PatternMatcher::ThreatLevel::HIGH: patternScore = 0.7; break;
                case PatternMatcher::ThreatLevel::CRITICAL: patternScore = 0.9; break;
                default: break;
            }
            maxScore = std::max(maxScore, patternScore);
            alert.description = patternResult.description;
        }
        
        // Check content if it's a text file
        auto contentResult = patternMatcher_->checkContent(path);
        if (contentResult.matched) {
            alert.indicators.push_back("Content: " + contentResult.patternName);
            
            double contentScore = 0.0;
            switch (contentResult.level) {
                case PatternMatcher::ThreatLevel::LOW: contentScore = 0.3; break;
                case PatternMatcher::ThreatLevel::MEDIUM: contentScore = 0.5; break;
                case PatternMatcher::ThreatLevel::HIGH: contentScore = 0.7; break;
                case PatternMatcher::ThreatLevel::CRITICAL: contentScore = 0.9; break;
                default: break;
            }
            maxScore = std::max(maxScore, contentScore);
            if (alert.description.empty()) {
                alert.description = contentResult.description;
            }
        }
    }
    
    // Check entropy
    if (entropyAnalyzer_ && std::filesystem::is_regular_file(path)) {
        auto entropyResult = entropyAnalyzer_->analyzeFile(path);
        if (entropyResult.isAnomalous || entropyResult.isEncryptedLooking) {
            alert.indicators.push_back("Entropy: " + 
                std::to_string(entropyResult.entropy) + " bits");
            
            double entropyScore = entropyResult.isEncryptedLooking ? 0.8 : 
                                  (entropyResult.isAnomalous ? 0.6 : 0.0);
            maxScore = std::max(maxScore, entropyScore);
            
            if (!entropyResult.reason.empty()) {
                if (!alert.description.empty()) alert.description += "; ";
                alert.description += entropyResult.reason;
            }
        }
    }
    
    // Determine threat type and severity
    if (maxScore > 0) {
        alert.confidenceScore = maxScore;
        alert.severity = scoreSeverity(maxScore);
        alert.type = ThreatType::SUSPICIOUS_ACTIVITY;
        
        // Refine type based on indicators
        for (const auto& indicator : alert.indicators) {
            if (indicator.find("RANSOMWARE") != std::string::npos ||
                indicator.find("RANSOM") != std::string::npos ||
                indicator.find("Entropy") != std::string::npos) {
                alert.type = ThreatType::RANSOMWARE;
                break;
            }
        }
        
        // Set recommended action
        if (alert.severity >= Severity::HIGH) {
            alert.recommendedAction = "Quarantine file and investigate immediately";
        } else if (alert.severity == Severity::MEDIUM) {
            alert.recommendedAction = "Review file and monitor for additional suspicious activity";
        } else {
            alert.recommendedAction = "Monitor for further suspicious activity";
        }
    }
    
    return alert;
}

ThreatDetector::Severity ThreatDetector::getCurrentThreatLevel() const {
    return scoreSeverity(currentThreatScore_);
}

double ThreatDetector::getCurrentThreatScore() const {
    return currentThreatScore_;
}

void ThreatDetector::setAlertCallback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    alertCallback_ = callback;
}

void ThreatDetector::reportFalsePositive(const ThreatAlert& alert) {
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.falsePositivesReported++;
    
    // TODO: Use false positive info to tune detection thresholds
}

ThreatDetector::DetectionStats ThreatDetector::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

std::vector<ThreatDetector::ThreatAlert> ThreatDetector::getRecentAlerts(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ThreatAlert> alerts;
    size_t count = std::min(maxCount, recentAlerts_.size());
    
    for (size_t i = 0; i < count; ++i) {
        alerts.push_back(recentAlerts_[recentAlerts_.size() - 1 - i]);
    }
    
    return alerts;
}

bool ThreatDetector::saveProfiles() {
    if (config_.profilePath.empty()) return false;
    
    std::filesystem::create_directories(config_.profilePath);
    bool success = true;
    
    if (behaviorProfiler_) {
        success &= behaviorProfiler_->saveProfile(
            config_.profilePath + "/behavior_profile.dat");
    }
    
    if (entropyAnalyzer_) {
        success &= entropyAnalyzer_->saveBaselines(
            config_.profilePath + "/entropy_baselines.dat");
    }
    
    return success;
}

bool ThreatDetector::loadProfiles() {
    if (config_.profilePath.empty()) return false;
    
    bool success = true;
    
    if (behaviorProfiler_) {
        behaviorProfiler_->loadProfile(
            config_.profilePath + "/behavior_profile.dat");
    }
    
    if (entropyAnalyzer_) {
        entropyAnalyzer_->loadBaselines(
            config_.profilePath + "/entropy_baselines.dat");
    }
    
    return success;
}

void ThreatDetector::updateModels() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isolationForest_ || eventHistory_.size() < 100) return;
    
    // Prepare training data
    std::vector<std::vector<double>> trainingData;
    
    // Group events into windows and extract features
    const int windowSize = 20;  // events per window
    
    for (size_t i = 0; i + windowSize <= eventHistory_.size(); i += windowSize / 2) {
        std::vector<std::tuple<std::string, std::string, size_t, double>> windowEvents;
        
        for (size_t j = i; j < i + windowSize && j < eventHistory_.size(); ++j) {
            const auto& event = eventHistory_[j];
            windowEvents.emplace_back(event.action, event.path, event.size, event.entropy);
        }
        
        auto features = FeatureExtractor::extractFeatures(windowEvents);
        trainingData.push_back(features.toVector());
    }
    
    if (trainingData.size() >= 50) {
        isolationForest_->fit(trainingData);
    }
}

ThreatDetector::ComponentStatus ThreatDetector::getComponentStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ComponentStatus status;
    
    if (behaviorProfiler_) {
        status.behaviorProfilerReady = behaviorProfiler_->isProfileReady();
        status.behaviorLearningProgress = behaviorProfiler_->getLearningProgress();
    }
    
    if (isolationForest_) {
        status.isolationForestTrained = isolationForest_->isTrained();
    }
    
    if (patternMatcher_) {
        status.patternsLoaded = PatternMatcher::getKnownRansomwareExtensions().size() +
                                PatternMatcher::getKnownRansomNoteNames().size();
    }
    
    return status;
}

std::string ThreatDetector::threatTypeToString(ThreatType type) {
    switch (type) {
        case ThreatType::NONE: return "NONE";
        case ThreatType::RANSOMWARE: return "RANSOMWARE";
        case ThreatType::DATA_EXFILTRATION: return "DATA_EXFILTRATION";
        case ThreatType::MASS_DELETION: return "MASS_DELETION";
        case ThreatType::SUSPICIOUS_ACTIVITY: return "SUSPICIOUS_ACTIVITY";
        case ThreatType::UNKNOWN_ANOMALY: return "UNKNOWN_ANOMALY";
        default: return "UNKNOWN";
    }
}

std::string ThreatDetector::severityToString(Severity severity) {
    switch (severity) {
        case Severity::NONE: return "NONE";
        case Severity::LOW: return "LOW";
        case Severity::MEDIUM: return "MEDIUM";
        case Severity::HIGH: return "HIGH";
        case Severity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

ThreatDetector::ThreatAlert ThreatDetector::combineDetectionResults(
    double ruleBasedScore,
    const BehaviorProfiler::AnomalyResult& behaviorResult,
    const FileEntropyAnalyzer::EntropyResult& entropyResult,
    const PatternMatcher::PatternMatch& patternResult,
    double isolationForestScore,
    const std::string& path) {
    
    ThreatAlert alert;
    alert.timestamp = std::chrono::system_clock::now();
    
    // Calculate weighted combined score
    double combinedScore = 0.0;
    double totalWeight = 0.0;
    
    // Rule-based (weight: 1.0)
    if (config_.enableRuleBasedDetection && ruleBasedScore > 0) {
        combinedScore += ruleBasedScore * 1.0;
        totalWeight += 1.0;
        alert.indicators.push_back("Rule-based score: " + 
            std::to_string(static_cast<int>(ruleBasedScore * 100)) + "%");
    }
    
    // Behavior profiling (weight: 1.5)
    if (config_.enableBehaviorProfiling && behaviorResult.isAnomaly) {
        combinedScore += behaviorResult.score * 1.5;
        totalWeight += 1.5;
        alert.indicators.push_back("Behavior anomaly: " + behaviorResult.reason);
    }
    
    // Entropy analysis (weight: 2.0 for encrypted-looking files)
    if (config_.enableEntropyAnalysis) {
        if (entropyResult.isEncryptedLooking) {
            combinedScore += 0.9 * 2.0;
            totalWeight += 2.0;
            alert.indicators.push_back("Encrypted-looking file (entropy: " + 
                std::to_string(entropyResult.entropy) + ")");
        } else if (entropyResult.isAnomalous) {
            combinedScore += 0.6 * 1.5;
            totalWeight += 1.5;
            alert.indicators.push_back("Anomalous entropy: " + entropyResult.reason);
        }
    }
    
    // Pattern matching (weight: 2.5 for critical patterns)
    if (config_.enablePatternMatching && patternResult.matched) {
        double patternScore = 0.0;
        double patternWeight = 1.0;
        
        switch (patternResult.level) {
            case PatternMatcher::ThreatLevel::LOW:
                patternScore = 0.3; patternWeight = 1.0; break;
            case PatternMatcher::ThreatLevel::MEDIUM:
                patternScore = 0.5; patternWeight = 1.5; break;
            case PatternMatcher::ThreatLevel::HIGH:
                patternScore = 0.7; patternWeight = 2.0; break;
            case PatternMatcher::ThreatLevel::CRITICAL:
                patternScore = 0.95; patternWeight = 2.5; break;
            default: break;
        }
        
        combinedScore += patternScore * patternWeight;
        totalWeight += patternWeight;
        alert.indicators.push_back("Pattern match: " + patternResult.patternName);
    }
    
    // Isolation Forest (weight: 1.0)
    if (config_.enableIsolationForest && isolationForestScore > 0.5) {
        combinedScore += isolationForestScore * 1.0;
        totalWeight += 1.0;
        alert.indicators.push_back("ML anomaly score: " + 
            std::to_string(static_cast<int>(isolationForestScore * 100)) + "%");
    }
    
    // Calculate final score
    if (totalWeight > 0) {
        alert.confidenceScore = combinedScore / totalWeight;
    }
    
    // Apply sensitivity adjustment
    alert.confidenceScore *= (0.5 + config_.sensitivityLevel);
    alert.confidenceScore = std::min(1.0, alert.confidenceScore);
    
    // Classify threat
    alert.type = classifyThreat(behaviorResult, entropyResult, patternResult, "");
    alert.severity = scoreSeverity(alert.confidenceScore);
    
    // Set description
    if (patternResult.matched) {
        alert.description = patternResult.description;
    } else if (entropyResult.isAnomalous) {
        alert.description = entropyResult.reason;
    } else if (behaviorResult.isAnomaly) {
        alert.description = behaviorResult.reason;
    } else if (ruleBasedScore > 0.5) {
        alert.description = "Suspicious activity rate detected";
    }
    
    // Set recommended action
    switch (alert.severity) {
        case Severity::CRITICAL:
            alert.recommendedAction = "IMMEDIATE ACTION REQUIRED: Possible active threat. "
                                      "Consider isolating system and investigating.";
            break;
        case Severity::HIGH:
            alert.recommendedAction = "Investigate immediately and review affected files";
            break;
        case Severity::MEDIUM:
            alert.recommendedAction = "Monitor closely and review recent file changes";
            break;
        case Severity::LOW:
            alert.recommendedAction = "Continue monitoring";
            break;
        default:
            break;
    }
    
    return alert;
}

ThreatDetector::ThreatType ThreatDetector::classifyThreat(
    const BehaviorProfiler::AnomalyResult& behaviorResult,
    const FileEntropyAnalyzer::EntropyResult& entropyResult,
    const PatternMatcher::PatternMatch& patternResult,
    const std::string& action) {
    
    // Ransomware indicators
    if (patternResult.matched && 
        (patternResult.patternName.find("RANSOMWARE") != std::string::npos ||
         patternResult.patternName.find("RANSOM") != std::string::npos)) {
        return ThreatType::RANSOMWARE;
    }
    
    if (entropyResult.isEncryptedLooking || 
        (entropyResult.isAnomalous && entropyResult.entropy > 7.5)) {
        return ThreatType::RANSOMWARE;
    }
    
    if (patternResult.matched && patternResult.patternName == "MASS_RENAME") {
        return ThreatType::RANSOMWARE;
    }
    
    // Mass deletion
    if (behaviorResult.isAnomaly && 
        behaviorResult.category == "RATE" &&
        action == "DELETE") {
        return ThreatType::MASS_DELETION;
    }
    
    // General suspicious activity
    if (behaviorResult.isAnomaly || patternResult.matched) {
        return ThreatType::SUSPICIOUS_ACTIVITY;
    }
    
    // Unknown anomaly
    if (entropyResult.isAnomalous) {
        return ThreatType::UNKNOWN_ANOMALY;
    }
    
    return ThreatType::NONE;
}

ThreatDetector::Severity ThreatDetector::scoreSeverity(double score) const {
    if (score >= 0.9) return Severity::CRITICAL;
    if (score >= 0.7) return Severity::HIGH;
    if (score >= 0.5) return Severity::MEDIUM;
    if (score >= 0.3) return Severity::LOW;
    return Severity::NONE;
}

void ThreatDetector::recordEvent(const std::string& action, const std::string& path,
                                  size_t size, double entropy) {
    EventRecord record;
    record.action = action;
    record.path = path;
    record.size = size;
    record.entropy = entropy;
    record.timestamp = std::chrono::steady_clock::now();
    
    eventHistory_.push_back(record);
    pruneEventHistory();
}

void ThreatDetector::pruneEventHistory() {
    while (eventHistory_.size() > MAX_EVENT_HISTORY) {
        eventHistory_.pop_front();
    }
}

bool ThreatDetector::shouldAlert() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastAlertTime_).count();
    
    if (elapsed >= 60) {
        return true;  // New minute window
    }
    
    return alertsThisMinute_ < config_.maxAlertsPerMinute;
}

void ThreatDetector::generateAlert(const ThreatAlert& alert) {
    if (!shouldAlert()) return;
    
    // Update alert tracking
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastAlertTime_).count();
    
    if (elapsed >= 60) {
        alertsThisMinute_ = 0;
        lastAlertTime_ = now;
    }
    alertsThisMinute_++;
    
    // Store alert
    recentAlerts_.push_back(alert);
    while (recentAlerts_.size() > MAX_RECENT_ALERTS) {
        recentAlerts_.pop_front();
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.alertsGenerated++;
        stats_.alertsByType[alert.type]++;
    }
    
    // Call callback
    if (alertCallback_) {
        alertCallback_(alert);
    }
    
    // Console output for critical alerts
    if (alert.severity >= Severity::HIGH) {
        std::cout << "🚨 THREAT ALERT [" << severityToString(alert.severity) << "]: "
                  << threatTypeToString(alert.type) << std::endl;
        std::cout << "   " << alert.description << std::endl;
        std::cout << "   Recommended: " << alert.recommendedAction << std::endl;
    }
}

} // namespace SentinelFS
