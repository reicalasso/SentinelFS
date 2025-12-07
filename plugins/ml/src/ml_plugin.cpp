#include "IPlugin.h"
#include "EventBus.h"
#include "ThreatDetector.h"
#include "Logger.h"
#include <iostream>
#include <memory>

namespace SentinelFS {

/**
 * @brief Advanced ML-based threat detection plugin
 * 
 * Provides comprehensive threat detection using multiple ML techniques:
 * - Rule-based anomaly detection (rapid modifications, mass deletions)
 * - Behavioral profiling (learns normal patterns, detects deviations)
 * - File entropy analysis (detects encrypted files - ransomware indicator)
 * - Pattern matching (known ransomware signatures, ransom notes)
 * - Isolation Forest (unsupervised anomaly detection)
 * 
 * Produces unified threat scores and alerts for:
 * - Ransomware attacks
 * - Mass file deletions
 * - Data exfiltration attempts
 * - General suspicious activity
 */
class MLPlugin : public IPlugin {
public:
    MLPlugin() = default;

    bool initialize(EventBus* eventBus) override {
        auto& logger = Logger::instance();
        eventBus_ = eventBus;
        
        // Configure threat detector
        ThreatDetector::Config config;
        config.enableBehaviorProfiling = true;
        config.enableEntropyAnalysis = true;
        config.enablePatternMatching = true;
        config.enableIsolationForest = true;
        config.enableRuleBasedDetection = true;
        config.alertThreshold = 0.5;
        config.sensitivityLevel = 0.5;
        
        // Profile path (relative to data directory)
        config.profilePath = "ml_profiles";
        config.autoSaveProfiles = true;
        
        detector_ = std::make_unique<ThreatDetector>(config);
        
        if (!detector_->initialize()) {
            logger.error("Failed to initialize ThreatDetector", "MLPlugin");
            return false;
        }
        
        // Set up alert callback
        detector_->setAlertCallback([this](const ThreatDetector::ThreatAlert& alert) {
            handleAlert(alert);
        });
        
        // Subscribe to file events
        eventBus_->subscribe("FILE_CREATED", [this](const std::any& data) {
            handleFileEvent("CREATE", data);
        });
        
        eventBus_->subscribe("FILE_MODIFIED", [this](const std::any& data) {
            handleFileEvent("MODIFY", data);
        });
        
        eventBus_->subscribe("FILE_DELETED", [this](const std::any& data) {
            handleFileEvent("DELETE", data);
        });
        
        // Subscribe to sync events for additional monitoring
        eventBus_->subscribe("SYNC_STARTED", [this](const std::any&) {
            // Could adjust sensitivity during sync
        });
        
        logger.info("MLPlugin initialized with advanced threat detection", "MLPlugin");
        logger.info("  - Behavior profiling: enabled", "MLPlugin");
        logger.info("  - Entropy analysis: enabled", "MLPlugin");
        logger.info("  - Pattern matching: enabled", "MLPlugin");
        logger.info("  - Isolation Forest: enabled", "MLPlugin");
        
        return true;
    }

    void shutdown() override {
        auto& logger = Logger::instance();
        
        if (detector_) {
            // Get final statistics
            auto stats = detector_->getStats();
            logger.info("MLPlugin shutdown - Statistics:", "MLPlugin");
            logger.info("  Total events processed: " + std::to_string(stats.totalEventsProcessed), "MLPlugin");
            logger.info("  Alerts generated: " + std::to_string(stats.alertsGenerated), "MLPlugin");
            logger.info("  False positives reported: " + std::to_string(stats.falsePositivesReported), "MLPlugin");
            
            detector_->shutdown();
        }
    }

    std::string getName() const override {
        return "MLPlugin";
    }

    std::string getVersion() const override {
        return "2.0.0-advanced-threat-detection";
    }

    /**
     * @brief Get current threat level for status reporting
     */
    std::string getThreatLevel() const {
        if (!detector_) return "UNKNOWN";
        return ThreatDetector::severityToString(detector_->getCurrentThreatLevel());
    }

    /**
     * @brief Get current threat score (0-1)
     */
    double getThreatScore() const {
        if (!detector_) return 0.0;
        return detector_->getCurrentThreatScore();
    }

    /**
     * @brief Get component status
     */
    ThreatDetector::ComponentStatus getComponentStatus() const {
        if (!detector_) return {};
        return detector_->getComponentStatus();
    }

    /**
     * @brief Analyze a specific file
     */
    ThreatDetector::ThreatAlert analyzeFile(const std::string& path) {
        if (!detector_) return {};
        return detector_->analyzeFile(path);
    }

private:
    EventBus* eventBus_{nullptr};
    std::unique_ptr<ThreatDetector> detector_;

    /**
     * @brief Handle file system event
     */
    void handleFileEvent(const std::string& action, const std::any& data) {
        if (!detector_) return;
        
        try {
            std::string path = std::any_cast<std::string>(data);
            
            // Process event through threat detector
            auto alert = detector_->processEvent(action, path);
            
            // Log high-severity detections
            if (alert.severity >= ThreatDetector::Severity::MEDIUM) {
                auto& logger = Logger::instance();
                logger.warn("Threat detected: " + alert.description, "MLPlugin");
            }
            
        } catch (const std::bad_any_cast&) {
            // Ignore invalid data
        }
    }

    /**
     * @brief Handle threat alert from detector
     */
    void handleAlert(const ThreatDetector::ThreatAlert& alert) {
        if (!eventBus_) return;
        
        auto& logger = Logger::instance();
        
        // Publish to EventBus
        std::string alertType = ThreatDetector::threatTypeToString(alert.type);
        eventBus_->publish("ANOMALY_DETECTED", alertType);
        
        // Publish detailed alert as JSON-like string
        std::string alertDetails = "{";
        alertDetails += "\"type\":\"" + alertType + "\",";
        alertDetails += "\"severity\":\"" + ThreatDetector::severityToString(alert.severity) + "\",";
        alertDetails += "\"confidence\":" + std::to_string(alert.confidenceScore) + ",";
        alertDetails += "\"description\":\"" + alert.description + "\"";
        alertDetails += "}";
        
        eventBus_->publish("THREAT_ALERT", alertDetails);
        
        // Log based on severity
        switch (alert.severity) {
            case ThreatDetector::Severity::CRITICAL:
                logger.error("🚨 CRITICAL THREAT: " + alert.description, "MLPlugin");
                break;
            case ThreatDetector::Severity::HIGH:
                logger.warn("⚠️  HIGH THREAT: " + alert.description, "MLPlugin");
                break;
            case ThreatDetector::Severity::MEDIUM:
                logger.warn("⚡ MEDIUM THREAT: " + alert.description, "MLPlugin");
                break;
            default:
                logger.info("ℹ️  Low severity alert: " + alert.description, "MLPlugin");
                break;
        }
        
        // For critical threats, could trigger additional actions
        if (alert.severity == ThreatDetector::Severity::CRITICAL) {
            eventBus_->publish("CRITICAL_THREAT", alert.recommendedAction);
        }
    }
};

extern "C" {
    IPlugin* create_plugin() {
        return new MLPlugin();
    }

    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace SentinelFS
