#pragma once

/**
 * @file Zer0Plugin.h
 * @brief Zer0 - Advanced Threat Detection Plugin
 * 
 * Features:
 * - File type awareness (no false positives on compressed files)
 * - Magic byte validation (detect extension spoofing)
 * - Behavioral analysis (detect ransomware patterns)
 * - Process correlation (who modified the file?)
 * - Adaptive thresholds based on file type
 * - Whitelist/Blacklist system
 */

#include "IPlugin.h"
#include "MLEngine.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <functional>
#include <chrono>
#include <atomic>

// Forward declarations for new components
namespace SentinelFS {
namespace Zer0 {
    class YaraScanner;
    class ProcessMonitor;
    class AutoResponse;
}
}

namespace SentinelFS {

class EventBus;
class IStorageAPI;

namespace Zer0 {

/**
 * @brief Threat severity levels
 */
enum class ThreatLevel {
    NONE = 0,       // No threat
    INFO,           // Informational
    LOW,            // Low risk - monitor
    MEDIUM,         // Medium risk - warn user
    HIGH,           // High risk - block/quarantine
    CRITICAL        // Critical - immediate action
};

/**
 * @brief File category for analysis
 */
enum class FileCategory {
    UNKNOWN,
    TEXT,           // Plain text, source code
    DOCUMENT,       // Office docs, PDFs
    IMAGE,          // PNG, JPEG, GIF, etc.
    VIDEO,          // MP4, AVI, etc.
    AUDIO,          // MP3, WAV, etc.
    ARCHIVE,        // ZIP, TAR, etc.
    EXECUTABLE,     // EXE, ELF, scripts
    DATABASE,       // SQLite, etc.
    CONFIG          // JSON, YAML, INI
};

/**
 * @brief Threat type classification
 */
enum class ThreatType {
    NONE,
    EXTENSION_MISMATCH,     // Extension doesn't match content
    HIDDEN_EXECUTABLE,      // Executable disguised as other type
    HIGH_ENTROPY_TEXT,      // Encrypted/obfuscated text file
    RANSOMWARE_PATTERN,     // Ransomware behavior detected
    MASS_MODIFICATION,      // Too many files changed quickly
    SUSPICIOUS_RENAME,      // Suspicious rename pattern
    KNOWN_MALWARE_HASH,     // Hash matches known malware
    ANOMALOUS_BEHAVIOR,     // Unusual file access pattern
    DOUBLE_EXTENSION,       // file.pdf.exe pattern
    SCRIPT_IN_DATA          // Script embedded in data file
};

/**
 * @brief Detection result
 */
struct DetectionResult {
    ThreatLevel level{ThreatLevel::NONE};
    ThreatType type{ThreatType::NONE};
    std::string description;
    double confidence{0.0};     // 0.0 - 1.0
    std::string filePath;
    std::string fileHash;
    FileCategory category{FileCategory::UNKNOWN};
    double entropy{0.0};        // File entropy for ransomware detection
    
    // Process info (if available)
    pid_t pid{0};
    std::string processName;
    
    // Metadata
    std::chrono::steady_clock::time_point timestamp;
    std::map<std::string, std::string> details;
};

/**
 * @brief Magic byte signature
 */
struct MagicSignature {
    std::vector<uint8_t> bytes;
    size_t offset{0};
    FileCategory category;
    std::string description;
};

/**
 * @brief Behavioral event for pattern analysis
 */
struct BehaviorEvent {
    std::string path;
    std::string eventType;  // CREATE, MODIFY, DELETE, RENAME
    pid_t pid{0};
    std::string processName;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Configuration for Zer0
 */
struct Zer0Config {
    // Entropy thresholds by category
    std::map<FileCategory, double> entropyThresholds{
        {FileCategory::TEXT, 6.0},
        {FileCategory::DOCUMENT, 7.5},
        {FileCategory::CONFIG, 5.5},
        {FileCategory::EXECUTABLE, 7.0},
        {FileCategory::UNKNOWN, 7.8}
    };
    
    // Behavioral analysis
    int massModificationThreshold{50};      // Files per minute
    int suspiciousRenameThreshold{10};      // Renames per minute
    std::chrono::seconds behaviorWindow{60};
    
    // Whitelist
    std::set<std::string> whitelistedPaths;
    std::set<std::string> whitelistedProcesses;
    std::set<std::string> whitelistedHashes;
    
    // Actions
    bool autoQuarantine{true};
    bool notifyUser{true};
    ThreatLevel quarantineThreshold{ThreatLevel::HIGH};
};

/**
 * @brief Detection callback
 */
using DetectionCallback = std::function<void(const DetectionResult&)>;

} // namespace Zer0

/**
 * @brief Zer0 main plugin class
 */
class Zer0Plugin : public IPlugin {
public:
    Zer0Plugin();
    ~Zer0Plugin() override;
    
    // IPlugin interface
    bool initialize(EventBus* eventBus) override;
    void shutdown() override;
    std::string getName() const override { return "Zer0"; }
    std::string getVersion() const override { return "1.0.0"; }
    
    // Storage reference
    void setStoragePlugin(IStorageAPI* storage);
    
    // Analysis API
    
    /**
     * @brief Analyze a file for threats
     */
    Zer0::DetectionResult analyzeFile(const std::string& path);
    
    /**
     * @brief Analyze file with process context
     */
    Zer0::DetectionResult analyzeFile(const std::string& path, pid_t pid, const std::string& processName);
    
    /**
     * @brief Record behavioral event
     */
    void recordEvent(const Zer0::BehaviorEvent& event);
    
    /**
     * @brief Check for behavioral anomalies
     */
    Zer0::DetectionResult checkBehavior();
    
    // File type detection
    
    /**
     * @brief Detect file category from content
     */
    Zer0::FileCategory detectFileCategory(const std::string& path);
    
    /**
     * @brief Validate magic bytes match extension
     */
    bool validateMagicBytes(const std::string& path);
    
    /**
     * @brief Check for double extension attack
     */
    bool hasDoubleExtension(const std::string& path);
    
    // Configuration
    
    void setConfig(const Zer0::Zer0Config& config);
    Zer0::Zer0Config getConfig() const;
    
    /**
     * @brief Add to whitelist
     */
    void whitelistPath(const std::string& path);
    void whitelistProcess(const std::string& processName);
    void whitelistHash(const std::string& hash);
    
    /**
     * @brief Set detection callback
     */
    void setDetectionCallback(Zer0::DetectionCallback callback);
    
    // Quarantine
    
    /**
     * @brief Quarantine a file
     */
    bool quarantineFile(const std::string& path);
    
    /**
     * @brief Restore from quarantine
     */
    bool restoreFile(const std::string& quarantinePath, const std::string& originalPath);
    
    /**
     * @brief Get quarantine list
     */
    std::vector<std::string> getQuarantineList() const;
    
    // Statistics
    
    struct Stats {
        uint64_t filesAnalyzed{0};
        uint64_t threatsDetected{0};
        uint64_t filesQuarantined{0};
        uint64_t falsePositives{0};
        std::map<Zer0::ThreatType, uint64_t> threatsByType;
        std::map<Zer0::ThreatLevel, uint64_t> threatsByLevel;
    };
    Stats getStats() const;
    
    // New component accessors
    
    /**
     * @brief Get ML Engine for anomaly detection
     */
    Zer0::MLEngine* getMLEngine();
    
    /**
     * @brief Get YARA Scanner for signature-based detection
     */
    Zer0::YaraScanner* getYaraScanner();
    
    /**
     * @brief Get Process Monitor for real-time process tracking
     */
    Zer0::ProcessMonitor* getProcessMonitor();
    
    /**
     * @brief Get Auto Response system for automated remediation
     */
    Zer0::AutoResponse* getAutoResponse();
    
    /**
     * @brief Perform comprehensive scan using all detection methods
     */
    Zer0::DetectionResult comprehensiveScan(const std::string& path);
    
    /**
     * @brief Start real-time monitoring
     */
    bool startMonitoring();
    
    /**
     * @brief Stop real-time monitoring
     */
    void stopMonitoring();
    
    /**
     * @brief Check if monitoring is active
     */
    bool isMonitoring() const;
    
    /**
     * @brief Train ML model from a directory
     * @param directoryPath Path to scan for training data
     * @return Number of files processed
     */
    int trainModel(const std::string& directoryPath);
    
    /**
     * @brief Get current training status
     */
    Zer0::MLEngine::TrainingStatus getTrainingStatus() const;
    
    /**
     * @brief Publish current status to EventBus
     */
    void publishStatus();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    void handleThreat(const Zer0::DetectionResult& result);
};

} // namespace SentinelFS
