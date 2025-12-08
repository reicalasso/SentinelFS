#include "IPlugin.h"
#include "EventBus.h"
#include "ThreatDetector.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include "IStorageAPI.h"
#include "PathUtils.h"
#include <iostream>
#include <memory>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <sqlite3.h>

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
        
        // Get storage reference via EventBus (storage plugin should be loaded first)
        storage_ = nullptr;
        
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
        
        // Subscribe to storage reference event
        eventBus_->subscribe("ML_SET_STORAGE", [this](const std::any& data) {
            try {
                storage_ = std::any_cast<IStorageAPI*>(data);
                if (storage_) {
                    // Ensure quarantine directory exists
                    auto quarantineDir = PathUtils::getDataDir() / "quarantine";
                    PathUtils::ensureDirectory(quarantineDir);
                    
                    auto& logger = Logger::instance();
                    logger.info("Storage reference received, quarantine directory ready", "MLPlugin");
                }
            } catch (const std::bad_any_cast&) {
                // Ignore invalid data
            }
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

    /**
     * @brief Set storage reference (called by daemon)
     */
    void setStorage(IStorageAPI* storage) {
        storage_ = storage;
        
        // Ensure quarantine directory exists
        auto quarantineDir = PathUtils::getDataDir() / "quarantine";
        PathUtils::ensureDirectory(quarantineDir);
        
        auto& logger = Logger::instance();
        logger.info("Quarantine directory: " + quarantineDir.string(), "MLPlugin");
    }

private:
    EventBus* eventBus_{nullptr};
    std::unique_ptr<ThreatDetector> detector_;
    IStorageAPI* storage_{nullptr};

    /**
     * @brief Quarantine a suspicious file
     */
    std::string quarantineFile(const std::string& filePath) {
        namespace fs = std::filesystem;
        
        try {
            if (!fs::exists(filePath)) {
                return "";
            }
            
            auto quarantineDir = PathUtils::getDataDir() / "quarantine";
            PathUtils::ensureDirectory(quarantineDir);
            
            // Generate unique quarantine filename
            auto timestamp = std::time(nullptr);
            auto filename = fs::path(filePath).filename();
            auto quarantinePath = quarantineDir / (std::to_string(timestamp) + "_" + filename.string());
            
            // Copy file to quarantine (keep original for now)
            fs::copy_file(filePath, quarantinePath, fs::copy_options::overwrite_existing);
            
            return quarantinePath.string();
        } catch (const std::exception& e) {
            auto& logger = Logger::instance();
            logger.error("Failed to quarantine file: " + std::string(e.what()), "MLPlugin");
            return "";
        }
    }
    
    /**
     * @brief Save threat to database
     */
    bool saveThreatToDatabase(const ThreatDetector::ThreatAlert& alert, 
                               const std::string& filePath,
                               const std::string& quarantinePath) {
        if (!storage_) return false;
        
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        if (!db) return false;
        
        // Get or create file_id from files table
        int fileId = 0;
        const char* getFileIdSql = "SELECT id FROM files WHERE path = ?;";
        sqlite3_stmt* fileStmt;
        if (sqlite3_prepare_v2(db, getFileIdSql, -1, &fileStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(fileStmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(fileStmt) == SQLITE_ROW) {
                fileId = sqlite3_column_int(fileStmt, 0);
            }
            sqlite3_finalize(fileStmt);
        }
        
        // If file doesn't exist, create it first
        if (fileId == 0) {
            const char* insertFileSql = "INSERT INTO files (path) VALUES (?);";
            sqlite3_stmt* insertStmt;
            if (sqlite3_prepare_v2(db, insertFileSql, -1, &insertStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(insertStmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(insertStmt) == SQLITE_DONE) {
                    fileId = static_cast<int>(sqlite3_last_insert_rowid(db));
                }
                sqlite3_finalize(insertStmt);
            }
        }
        
        if (fileId == 0) {
            return false;
        }
        
        // Check if threat already exists for this file (prevent duplicates)
        // Check both active threats (marked_safe=0) and safe-marked threats (marked_safe=1)
        const char* checkSql = "SELECT id, marked_safe FROM detected_threats WHERE file_id = ?";
        sqlite3_stmt* checkStmt;
        bool exists = false;
        bool isMarkedSafe = false;
        
        if (sqlite3_prepare_v2(db, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(checkStmt, 1, fileId);
            if (sqlite3_step(checkStmt) == SQLITE_ROW) {
                exists = true;
                isMarkedSafe = sqlite3_column_int(checkStmt, 1) == 1;
            }
            sqlite3_finalize(checkStmt);
        }
        
        // If threat already exists for this file, skip insertion
        // This includes files marked as safe - they should not be re-detected
        if (exists) {
            if (isMarkedSafe) {
                // File was marked safe, don't create new threat
                return true;
            }
            // Active threat already exists, skip duplicate
            return true;
        }
        
        const char* sql = "INSERT INTO detected_threats "
                         "(file_id, threat_type_id, threat_level_id, threat_score, detected_at, "
                         "entropy, file_size, hash, quarantine_path, ml_model_used, additional_info, marked_safe) "
                         "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        // Map threat type to threat_type_id
        // 1=ransomware, 2=malware, 3=suspicious, 4=entropy_anomaly, 5=rapid_modification, 6=mass_deletion
        int threatTypeId = 3; // default: suspicious
        switch (alert.type) {
            case ThreatDetector::ThreatType::RANSOMWARE:
                threatTypeId = 1;
                break;
            case ThreatDetector::ThreatType::MASS_DELETION:
                threatTypeId = 6;
                break;
            case ThreatDetector::ThreatType::DATA_EXFILTRATION:
                threatTypeId = 3;
                break;
            default:
                threatTypeId = 3;
        }
        
        // Map severity to threat_level_id
        // 1=low, 2=medium, 3=high, 4=critical
        int threatLevelId = 1; // default: low
        switch (alert.severity) {
            case ThreatDetector::Severity::CRITICAL:
                threatLevelId = 4;
                break;
            case ThreatDetector::Severity::HIGH:
                threatLevelId = 3;
                break;
            case ThreatDetector::Severity::MEDIUM:
                threatLevelId = 2;
                break;
            default:
                threatLevelId = 1;
        }
        
        // Get file size
        int64_t fileSize = 0;
        try {
            if (std::filesystem::exists(filePath)) {
                fileSize = std::filesystem::file_size(filePath);
            }
        } catch (...) {}
        
        sqlite3_bind_int(stmt, 1, fileId);
        sqlite3_bind_int(stmt, 2, threatTypeId);
        sqlite3_bind_int(stmt, 3, threatLevelId);
        sqlite3_bind_double(stmt, 4, alert.confidenceScore * 100.0);
        sqlite3_bind_int64(stmt, 5, std::time(nullptr) * 1000LL); // milliseconds
        sqlite3_bind_double(stmt, 6, alert.fileEntropy);
        sqlite3_bind_int64(stmt, 7, fileSize);
        sqlite3_bind_null(stmt, 8); // hash (TODO: calculate if needed)
        sqlite3_bind_text(stmt, 9, quarantinePath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 10, "ThreatDetector_v2", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, alert.description.c_str(), -1, SQLITE_TRANSIENT);
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        return success;
    }

    /**
     * @brief Handle file system event
     */
    void handleFileEvent(const std::string& action, const std::any& data) {
        if (!detector_) return;
        
        try {
            std::string path = std::any_cast<std::string>(data);
            
            auto& logger = Logger::instance();
            auto& metrics = MetricsCollector::instance();
            
            logger.info("MLPlugin received event: " + action + " -> " + path, "MLPlugin");
            
            // Process event through threat detector
            auto alert = detector_->processEvent(action, path);
            
            // Update entropy metrics if we got a valid entropy value
            if (alert.fileEntropy > 0) {
                metrics.updateAvgFileEntropy(alert.fileEntropy);
                
                // Track high entropy files
                if (alert.fileEntropy > 7.0) {
                    metrics.incrementHighEntropyFiles();
                }
            }
            
            // Log detection results
            logger.info("MLPlugin detection result: severity=" + 
                       ThreatDetector::severityToString(alert.severity) + 
                       ", score=" + std::to_string(alert.confidenceScore) +
                       ", entropy=" + std::to_string(alert.fileEntropy), "MLPlugin");
            
            // Log high-severity detections
            if (alert.severity >= ThreatDetector::Severity::MEDIUM) {
                logger.warn("Threat detected: " + alert.description, "MLPlugin");
                
                // Handle threat with file path context
                handleAlertWithPath(alert, path);
            }
            
        } catch (const std::bad_any_cast&) {
            // Ignore invalid data
        }
    }

    /**
     * @brief Handle threat alert from detector (called from callback - path unknown)
     */
    void handleAlert(const ThreatDetector::ThreatAlert& alert) {
        // This is called from the alert callback which doesn't have path info
        // Most alerts now come through handleAlertWithPath instead
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        // Update metrics
        metrics.incrementThreatsDetected();
        metrics.updateThreatScore(alert.confidenceScore);
        
        // Track specific threat types
        switch (alert.type) {
            case ThreatDetector::ThreatType::RANSOMWARE:
                metrics.incrementRansomwareAlerts();
                break;
            case ThreatDetector::ThreatType::MASS_DELETION:
                metrics.incrementMassOperationAlerts();
                break;
            default:
                metrics.incrementSuspiciousActivities();
                break;
        }
        
        // Publish to EventBus
        std::string alertType = ThreatDetector::threatTypeToString(alert.type);
        eventBus_->publish("ANOMALY_DETECTED", alertType);
    }
    
    /**
     * @brief Handle threat alert with file path context
     */
    void handleAlertWithPath(const ThreatDetector::ThreatAlert& alert, const std::string& filePath) {
        if (!eventBus_) return;
        
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        // Quarantine the file if severity is high enough
        std::string quarantinePath;
        if (alert.severity >= ThreatDetector::Severity::MEDIUM && !filePath.empty()) {
            quarantinePath = quarantineFile(filePath);
            if (!quarantinePath.empty()) {
                logger.info("File quarantined: " + filePath + " -> " + quarantinePath, "MLPlugin");
            }
        }
        
        // Save threat to database
        if (storage_ && !filePath.empty()) {
            if (saveThreatToDatabase(alert, filePath, quarantinePath)) {
                logger.info("Threat saved to database: " + filePath, "MLPlugin");
            } else {
                logger.error("Failed to save threat to database", "MLPlugin");
            }
        }
        
        // Update metrics
        metrics.incrementThreatsDetected();
        metrics.updateThreatScore(alert.confidenceScore);
        
        // Track specific threat types
        switch (alert.type) {
            case ThreatDetector::ThreatType::RANSOMWARE:
                metrics.incrementRansomwareAlerts();
                break;
            case ThreatDetector::ThreatType::MASS_DELETION:
                metrics.incrementMassOperationAlerts();
                break;
            default:
                metrics.incrementSuspiciousActivities();
                break;
        }
        
        // Publish detailed alert with file path
        std::string alertType = ThreatDetector::threatTypeToString(alert.type);
        std::string alertDetails = "{";
        alertDetails += "\"type\":\"" + alertType + "\",";
        alertDetails += "\"severity\":\"" + ThreatDetector::severityToString(alert.severity) + "\",";
        alertDetails += "\"confidence\":" + std::to_string(alert.confidenceScore) + ",";
        alertDetails += "\"description\":\"" + alert.description + "\",";
        alertDetails += "\"filePath\":\"" + filePath + "\",";
        alertDetails += "\"quarantinePath\":\"" + quarantinePath + "\"";
        alertDetails += "}";
        
        eventBus_->publish("THREAT_ALERT", alertDetails);
        
        // Log based on severity
        switch (alert.severity) {
            case ThreatDetector::Severity::CRITICAL:
                logger.error("🚨 CRITICAL THREAT: " + alert.description + " [" + filePath + "]", "MLPlugin");
                break;
            case ThreatDetector::Severity::HIGH:
                logger.warn("⚠️  HIGH THREAT: " + alert.description + " [" + filePath + "]", "MLPlugin");
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
