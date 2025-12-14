#include "Zer0Plugin.h"
#include "MagicBytes.h"
#include "BehaviorAnalyzer.h"
#include "YaraEngine.h"
#include "MLEngine.h"
#include "ProcessMonitor.h"
#include "ResponseEngine.h"
#include "EventBus.h"
#include "IStorageAPI.h"
#include "Logger.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <sqlite3.h>
#include <any>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <cctype>

namespace SentinelFS {

// Remove using namespace Zer0 to avoid filesystem conflicts

/**
 * @brief Implementation class (PIMPL)
 */
class Zer0Plugin::Impl {
public:
    EventBus* eventBus{nullptr};
    IStorageAPI* storage{nullptr};
    
    Zer0::Zer0Config config;
    Zer0::DetectionCallback detectionCallback;
    
    std::unique_ptr<Zer0::BehaviorAnalyzer> behaviorAnalyzer;
    std::unique_ptr<Zer0::YaraEngine> yaraEngine;
    std::unique_ptr<Zer0::MLEngine> mlEngine;
    std::unique_ptr<Zer0::ProcessMonitor> processMonitor;
    std::unique_ptr<Zer0::ResponseEngine> responseEngine;
    
    Zer0::Stats stats;
    mutable std::mutex statsMutex;
    
    std::string quarantineDir;
    
    // Helper methods
    std::string getParentProcess(pid_t pid) {
        if (pid <= 0) return "";
        
        std::string statusPath = "/proc/" + std::to_string(pid) + "/status";
        std::ifstream statusFile(statusPath);
        
        if (statusFile.is_open()) {
            std::string line;
            while (std::getline(statusFile, line)) {
                if (line.starts_with("PPid:")) {
                    pid_t ppid = std::stoi(line.substr(6));
                    if (ppid > 0) {
                        std::string ppidPath = "/proc/" + std::to_string(ppid) + "/comm";
                        std::ifstream ppidFile(ppidPath);
                        if (ppidFile.is_open()) {
                            std::string name;
                            std::getline(ppidFile, name);
                            return name;
                        }
                    }
                    break;
                }
            }
        }
        
        return "";
    }
    
    // Helpers
    double calculateEntropy(const std::vector<uint8_t>& data) {
        if (data.empty()) return 0.0;
        
        std::array<uint64_t, 256> freq{};
        for (uint8_t byte : data) {
            freq[byte]++;
        }
        
        double entropy = 0.0;
        double size = static_cast<double>(data.size());
        
        for (int i = 0; i < 256; ++i) {
            if (freq[i] > 0) {
                double p = static_cast<double>(freq[i]) / size;
                entropy -= p * std::log2(p);
            }
        }
        
        return entropy;
    }
    
    std::string calculateHash(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return "";
        
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";
        
        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        
        char buffer[8192];
        while (file.read(buffer, sizeof(buffer))) {
            EVP_DigestUpdate(ctx, buffer, file.gcount());
        }
        if (file.gcount() > 0) {
            EVP_DigestUpdate(ctx, buffer, file.gcount());
        }
        
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen;
        EVP_DigestFinal_ex(ctx, hash, &hashLen);
        EVP_MD_CTX_free(ctx);
        
        std::ostringstream oss;
        for (unsigned int i = 0; i < hashLen; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return oss.str();
    }
    
    std::vector<uint8_t> readFileHeader(const std::string& path, size_t maxSize = 8192) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        
        std::vector<uint8_t> buffer(maxSize);
        file.read(reinterpret_cast<char*>(buffer.data()), maxSize);
        buffer.resize(file.gcount());
        return buffer;
    }
    
    std::string getExtension(const std::string& path) {
        size_t dotPos = path.rfind('.');
        if (dotPos == std::string::npos || dotPos == path.length() - 1) {
            return "";
        }
        return path.substr(dotPos + 1);
    }
    
    bool isWhitelisted(const std::string& path, pid_t pid, const std::string& processName) {
        // Check path whitelist
        for (const auto& wp : config.whitelistedPaths) {
            if (path.find(wp) == 0) return true;
        }
        
        // Check process whitelist
        if (!processName.empty() && config.whitelistedProcesses.count(processName) > 0) {
            return true;
        }
        
        return false;
    }
    
    double getEntropyThreshold(FileCategory category) {
        auto it = config.entropyThresholds.find(category);
        if (it != config.entropyThresholds.end()) {
            return it->second;
        }
        return 7.8;  // Default high threshold
    }
};

// Main class implementation

Zer0Plugin::Zer0Plugin() : impl_(std::make_unique<Impl>()) {
    impl_->behaviorAnalyzer = std::make_unique<BehaviorAnalyzer>();
    
    // Set default quarantine directory
    const char* home = std::getenv("HOME");
    if (home) {
        impl_->quarantineDir = std::string(home) + "/.local/share/sentinelfs/zer0_quarantine";
    } else {
        impl_->quarantineDir = "/tmp/zer0_quarantine";
    }
}

Zer0Plugin::~Zer0Plugin() {
    shutdown();
}

bool Zer0Plugin::initialize(EventBus* eventBus) {
    auto& logger = Logger::instance();
    
    impl_->eventBus = eventBus;
    
    logger.log(LogLevel::INFO, "Initializing Zer0 threat detection plugin", "Zer0");
    
    // Create quarantine directory
    std::filesystem::create_directories(impl_->quarantineDir);
    
    // Initialize YARA Engine
    impl_->yaraEngine = std::make_unique<YaraEngine>();
    if (!impl_->yaraEngine->initialize()) {
        logger.log(LogLevel::WARNING, "YARA engine initialization failed, continuing without YARA", "Zer0");
    } else {
        // Load default rules
        if (!impl_->config.yaraRulesPath.empty()) {
            impl_->yaraEngine->loadRules(impl_->config.yaraRulesPath);
        }
    }
    
    // Initialize ML Engine
    impl_->mlEngine = std::make_unique<MLEngine>();
    if (!impl_->mlEngine->initialize(ModelType::ISOLATION_FOREST)) {
        logger.log(LogLevel::WARNING, "ML engine initialization failed", "Zer0");
    }
    
    // Initialize Process Monitor
    if (impl_->config.enableProcessMonitoring) {
        impl_->processMonitor = std::make_unique<ProcessMonitor>();
        ProcessMonitorConfig pmConfig;
        pmConfig.monitorAllProcesses = false;
        pmConfig.updateInterval = std::chrono::milliseconds(1000);
        
        if (impl_->processMonitor->initialize(pmConfig)) {
            impl_->processMonitor->start();
            logger.log(LogLevel::INFO, "Process monitoring started", "Zer0");
        }
    }
    
    // Initialize Response Engine
    impl_->responseEngine = std::make_unique<ResponseEngine>();
    if (!impl_->responseEngine->initialize()) {
        logger.log(LogLevel::ERROR, "Response engine initialization failed", "Zer0");
        return false;
    }
    
    // Start behavior analyzer
    impl_->behaviorAnalyzer->start(impl_->config.behaviorWindow);
    
    // Subscribe to file events
    if (eventBus) {
        eventBus->subscribe("FILE_CREATED", [this](const std::any& data) {
            try {
                std::string path = std::any_cast<std::string>(data);
                auto result = analyzeFile(path);
                if (result.level >= Zer0::ThreatLevel::MEDIUM) {
                    handleThreat(result);
                }
            } catch (...) {}
        });
        
        eventBus->subscribe("FILE_MODIFIED", [this](const std::any& data) {
            try {
                std::string path = std::any_cast<std::string>(data);
                auto result = analyzeFile(path);
                if (result.level >= Zer0::ThreatLevel::MEDIUM) {
                    handleThreat(result);
                }
            } catch (...) {}
        });
        
        eventBus->subscribe("FILE_RENAMED", [this](const std::any& data) {
            try {
                std::string path = std::any_cast<std::string>(data);
                // Record for behavioral analysis
                BehaviorEvent event;
                event.path = path;
                event.eventType = "RENAME";
                event.timestamp = std::chrono::steady_clock::now();
                impl_->behaviorAnalyzer->recordEvent(event);
                
                // Check behavior
                auto behaviorResult = checkBehavior();
                if (behaviorResult.level >= Zer0::ThreatLevel::MEDIUM) {
                    handleThreat(behaviorResult);
                }
            } catch (...) {}
        });
    }
    
    logger.log(LogLevel::INFO, "Zer0 initialized successfully", "Zer0");
    logger.log(LogLevel::INFO, "  - Magic byte validation: enabled", "Zer0");
    logger.log(LogLevel::INFO, "  - Behavioral analysis: enabled", "Zer0");
    logger.log(LogLevel::INFO, "  - File type awareness: enabled", "Zer0");
    logger.log(LogLevel::INFO, "  - YARA rules: " + 
        (impl_->yaraEngine ? "enabled" : "disabled"), "Zer0");
    logger.log(LogLevel::INFO, "  - ML anomaly detection: " + 
        (impl_->mlEngine ? "enabled" : "disabled"), "Zer0");
    logger.log(LogLevel::INFO, "  - Process monitoring: " + 
        (impl_->processMonitor ? "enabled" : "disabled"), "Zer0");
    logger.log(LogLevel::INFO, "  - Automated response: enabled", "Zer0");
    logger.log(LogLevel::INFO, "  - Quarantine directory: " + impl_->quarantineDir, "Zer0");
    
    return true;
}

void Zer0Plugin::shutdown() {
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Shutting down Zer0", "Zer0");
    
    if (impl_->behaviorAnalyzer) {
        impl_->behaviorAnalyzer->stop();
    }
    
    if (impl_->processMonitor) {
        impl_->processMonitor->stop();
    }
    
    // Other engines clean up automatically in destructors
}

void Zer0Plugin::setStoragePlugin(IStorageAPI* storage) {
    impl_->storage = storage;
}

DetectionResult Zer0Plugin::analyzeFile(const std::string& path) {
    return analyzeFile(path, 0, "");
}

DetectionResult Zer0Plugin::analyzeFile(const std::string& path, pid_t pid, const std::string& processName) {
    auto& logger = Logger::instance();
    
    Zer0::DetectionResult result;
    result.filePath = path;
    result.pid = pid;
    result.processName = processName;
    result.timestamp = std::chrono::steady_clock::now();
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
    }
    
    // Check whitelist
    if (impl_->isWhitelisted(path, pid, processName)) {
        result.level = Zer0::ThreatLevel::NONE;
        result.description = "Whitelisted";
        return result;
    }
    
    // Check if file exists
    if (access(path.c_str(), F_OK) != 0) {
        result.level = Zer0::ThreatLevel::NONE;
        result.description = "File does not exist";
        return result;
    }
    
    // Read file header
    auto header = impl_->readFileHeader(path);
    if (header.empty()) {
        result.level = Zer0::ThreatLevel::NONE;
        result.description = "Could not read file";
        return result;
    }
    
    // Detect file category from content
    auto& magicBytes = MagicBytes::instance();
    Zer0::FileCategory detectedCategory = magicBytes.detectCategory(header);
    result.category = detectedCategory;
    
    // Get expected category from extension
    std::string ext = impl_->getExtension(path);
    Zer0::FileCategory expectedCategory = magicBytes.getCategoryForExtension(ext);
    
    // Calculate hash
    result.fileHash = impl_->calculateHash(path);
    
    // Check for known malware hash
    if (impl_->config.whitelistedHashes.count(result.fileHash) > 0) {
        result.level = Zer0::ThreatLevel::NONE;
        result.description = "Known safe hash";
        return result;
    }
    
    // === THREAT DETECTION ===
    
    // 1. Check for double extension (file.pdf.exe)
    if (hasDoubleExtension(path)) {
        result.level = Zer0::ThreatLevel::HIGH;
        result.type = Zer0::ThreatType::DOUBLE_EXTENSION;
        result.confidence = 0.9;
        result.description = "Double extension detected (possible malware disguise)";
        result.details["pattern"] = "double_extension";
        
        logger.log(LogLevel::WARN, "⚠️  Double extension: " + path, "Zer0");
        return result;
    }
    
    // 2. Check for hidden executable
    if (expectedCategory != Zer0::FileCategory::EXECUTABLE && 
        expectedCategory != Zer0::FileCategory::UNKNOWN &&
        magicBytes.isExecutable(header)) {
        
        result.level = Zer0::ThreatLevel::CRITICAL;
        result.type = Zer0::ThreatType::HIDDEN_EXECUTABLE;
        result.confidence = 0.95;
        result.description = "Executable disguised as " + ext + " file";
        result.details["expected"] = ext;
        result.details["actual"] = "executable";
        
        logger.log(LogLevel::ERROR, "🚨 HIDDEN EXECUTABLE: " + path, "Zer0");
        return result;
    }
    
    // 3. Check for extension mismatch (non-critical cases)
    if (expectedCategory != Zer0::FileCategory::UNKNOWN && 
        detectedCategory != Zer0::FileCategory::UNKNOWN &&
        expectedCategory != detectedCategory) {
        
        // Skip benign mismatches:
        // - Office documents are ZIP archives (docx, xlsx, pptx, etc.)
        // - TEXT files detected as TEXT (scripts, configs, etc.)
        // - SVG files can be detected as TEXT (XML-based)
        bool isBenignMismatch = 
            (expectedCategory == Zer0::FileCategory::DOCUMENT && detectedCategory == Zer0::FileCategory::ARCHIVE) ||
            (expectedCategory == Zer0::FileCategory::TEXT && detectedCategory == Zer0::FileCategory::TEXT) ||
            (expectedCategory == Zer0::FileCategory::TEXT) ||  // All text-based extensions are safe
            (expectedCategory == Zer0::FileCategory::IMAGE && ext == "svg" && detectedCategory == Zer0::FileCategory::TEXT);
        
        if (!isBenignMismatch) {
            result.level = Zer0::ThreatLevel::MEDIUM;
            result.type = Zer0::ThreatType::EXTENSION_MISMATCH;
            result.confidence = 0.7;
            result.description = "File content doesn't match extension";
            result.details["expected_type"] = std::to_string(static_cast<int>(expectedCategory));
            result.details["detected_type"] = std::to_string(static_cast<int>(detectedCategory));
            
            logger.log(LogLevel::WARN, "⚠️  Extension mismatch: " + path, "Zer0");
            return result;
        }
    }
    
    // 4. Check for embedded scripts in data files
    if (detectedCategory == Zer0::FileCategory::IMAGE || 
        detectedCategory == Zer0::FileCategory::DOCUMENT ||
        detectedCategory == Zer0::FileCategory::ARCHIVE) {
        
        // Read more content for script detection
        auto fullContent = impl_->readFileHeader(path, 65536);
        if (magicBytes.hasEmbeddedScript(fullContent)) {
            result.level = Zer0::ThreatLevel::HIGH;
            result.type = Zer0::ThreatType::SCRIPT_IN_DATA;
            result.confidence = 0.85;
            result.description = "Embedded script detected in data file";
            
            logger.log(LogLevel::WARN, "⚠️  Embedded script: " + path, "Zer0");
            return result;
        }
    }
    
    // 5. Check for ransomware file extensions
    {
        static const std::set<std::string> ransomwareExtensions = {
            "encrypted", "locked", "crypto", "crypt", "enc", "crypted",
            "locky", "cerber", "zepto", "thor", "aesir", "zzzzz",
            "micro", "xxx", "ttt", "ecc", "ezz", "exx", "xyz",
            "aaa", "abc", "ccc", "vvv", "zzz", "r5a", "r4a",
            "hermes", "WNCRY", "WCRY", "WNCRYT", "wallet", "dharma",
            "onion", "legion", "szf", "good", "breaking_bad"
        };
        
        std::string extLower = ext;
        std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
        
        if (ransomwareExtensions.count(extLower) > 0) {
            result.level = Zer0::ThreatLevel::HIGH;
            result.type = Zer0::ThreatType::RANSOMWARE_PATTERN;
            result.confidence = 0.9;
            result.description = "Ransomware file extension detected: ." + ext;
            result.details["extension"] = ext;
            
            logger.log(LogLevel::WARN, "🔐 Ransomware extension: " + path, "Zer0");
            return result;
        }
    }
    
    // 6. Entropy analysis (only for text/config/unknown files)
    if (detectedCategory == Zer0::FileCategory::TEXT ||
        detectedCategory == Zer0::FileCategory::CONFIG ||
        detectedCategory == Zer0::FileCategory::UNKNOWN) {
        
        double entropy = impl_->calculateEntropy(header);
        double threshold = impl_->getEntropyThreshold(detectedCategory);
        
        if (entropy > threshold) {
            result.level = Zer0::ThreatLevel::MEDIUM;
            result.type = Zer0::ThreatType::HIGH_ENTROPY_TEXT;
            result.confidence = std::min(1.0, (entropy - threshold) / 2.0 + 0.5);
            result.description = "High entropy in text file (possibly encrypted/obfuscated)";
            result.details["entropy"] = std::to_string(entropy);
            result.details["threshold"] = std::to_string(threshold);
            
            logger.log(LogLevel::WARN, "⚠️  High entropy text: " + path + 
                       " (entropy: " + std::to_string(entropy) + ")", "Zer0");
            return result;
        }
    }
    
    // 7. YARA rules scanning
    if (impl_->yaraEngine) {
        auto yaraResults = impl_->yaraEngine->scanFile(path);
        if (!yaraResults.empty()) {
            result.level = Zer0::ThreatLevel::HIGH;
            result.type = Zer0::ThreatType::YARA_MATCH;
            result.confidence = 0.95;
            result.description = "YARA rule match detected";
            
            for (const auto& yaraResult : yaraResults) {
                result.matchedRules.push_back(yaraResult.ruleName);
                result.yaraMetadata["rule_" + yaraResult.ruleName] = yaraResult.ruleDescription;
            }
            
            logger.log(LogLevel::ERROR, "🚨 YARA match: " + path, "Zer0");
            
            // Update stats
            {
                std::lock_guard<std::mutex> lock(impl_->statsMutex);
            }
            
            return result;
        }
    }
    
    // 8. ML anomaly detection
    if (impl_->mlEngine && impl_->config.enableML) {
        auto features = impl_->mlEngine->extractFeatures(path, pid, processName);
        auto mlResult = impl_->mlEngine->analyze(features);
        
        if (mlResult.isAnomaly) {
            result.level = Zer0::ThreatLevel::MEDIUM;
            result.type = Zer0::ThreatType::ML_ANOMALY;
            result.confidence = mlResult.confidence;
            result.description = "ML anomaly detected: " + mlResult.modelUsed;
            result.anomalyScore = mlResult.anomalyScore;
            result.featureVector = features.toVector();
            
            for (const auto& feature : mlResult.suspiciousFeatures) {
                result.details["ml_feature_" + feature] = "suspicious";
            }
            
            logger.log(LogLevel::WARN, "⚠️  ML anomaly: " + path + 
                       " (score: " + std::to_string(mlResult.anomalyScore) + ")", "Zer0");
            
            // Update stats
            {
                std::lock_guard<std::mutex> lock(impl_->statsMutex);
            }
            
            return result;
        }
    }
    
    // 9. Record event for behavioral analysis
    BehaviorEvent event;
    event.path = path;
    event.eventType = "MODIFY";
    event.pid = pid;
    event.processName = processName;
    event.parentProcess = impl_->getParentProcess(pid);
    event.timestamp = std::chrono::steady_clock::now();
    impl_->behaviorAnalyzer->recordEvent(event);
    
    // No threat detected
    result.level = Zer0::ThreatLevel::NONE;
    result.type = Zer0::ThreatType::NONE;
    result.description = "No threat detected";
    
    return result;
}

void Zer0Plugin::recordEvent(const BehaviorEvent& event) {
    impl_->behaviorAnalyzer->recordEvent(event);
}

DetectionResult Zer0Plugin::checkBehavior() {
    auto analysis = impl_->behaviorAnalyzer->analyze();
    
    DetectionResult result;
    result.level = analysis.threatLevel;
    result.confidence = analysis.confidence;
    result.description = analysis.description;
    result.timestamp = std::chrono::steady_clock::now();
    
    if (analysis.pattern == BehaviorPattern::EXTENSION_CHANGE ||
        analysis.pattern == BehaviorPattern::MASS_MODIFICATION ||
        analysis.pattern == BehaviorPattern::MASS_RENAME) {
        result.type = Zer0::ThreatType::RANSOMWARE_PATTERN;
    } else if (analysis.pattern == BehaviorPattern::SINGLE_PROCESS_STORM) {
        result.type = Zer0::ThreatType::ANOMALOUS_BEHAVIOR;
        result.pid = analysis.suspiciousPid;
        result.processName = analysis.suspiciousProcess;
    } else if (analysis.pattern == BehaviorPattern::MASS_DELETION) {
        result.type = Zer0::ThreatType::MASS_MODIFICATION;
    }
    
    return result;
}

FileCategory Zer0Plugin::detectFileCategory(const std::string& path) {
    auto header = impl_->readFileHeader(path);
    return MagicBytes::instance().detectCategory(header);
}

bool Zer0Plugin::validateMagicBytes(const std::string& path) {
    auto header = impl_->readFileHeader(path);
    std::string ext = impl_->getExtension(path);
    FileCategory expected = MagicBytes::instance().getCategoryForExtension(ext);
    return MagicBytes::instance().validateHeader(header, expected);
}

bool Zer0Plugin::hasDoubleExtension(const std::string& path) {
    std::string filename = std::filesystem::path(path).filename().string();
    
    // Common executable extensions
    static const std::set<std::string> execExtensions = {
        "exe", "bat", "cmd", "com", "msi", "scr", "pif",
        "sh", "bash", "py", "pl", "rb", "ps1", "vbs", "js"
    };
    
    // Find all dots
    std::vector<size_t> dots;
    for (size_t i = 0; i < filename.length(); ++i) {
        if (filename[i] == '.') {
            dots.push_back(i);
        }
    }
    
    if (dots.size() < 2) return false;
    
    // Get last extension
    std::string lastExt = filename.substr(dots.back() + 1);
    for (char& c : lastExt) {
        c = std::tolower(c);
    }
    
    // Check if last extension is executable
    if (execExtensions.count(lastExt) > 0) {
        // Get second-to-last extension
        std::string prevExt = filename.substr(dots[dots.size() - 2] + 1, 
                                               dots.back() - dots[dots.size() - 2] - 1);
        for (char& c : prevExt) {
            c = std::tolower(c);
        }
        
        // Common document extensions that might be spoofed
        static const std::set<std::string> docExtensions = {
            "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx",
            "txt", "rtf", "jpg", "jpeg", "png", "gif", "mp3", "mp4"
        };
        
        if (docExtensions.count(prevExt) > 0) {
            return true;
        }
    }
    
    return false;
}

void Zer0Plugin::setConfig(const Zer0Config& config) {
    impl_->config = config;
    
    impl_->behaviorAnalyzer->setMassModificationThreshold(config.massModificationThreshold);
    impl_->behaviorAnalyzer->setMassRenameThreshold(config.suspiciousRenameThreshold);
}

Zer0Config Zer0Plugin::getConfig() const {
    return impl_->config;
}

void Zer0Plugin::whitelistPath(const std::string& path) {
    impl_->config.whitelistedPaths.insert(path);
}

void Zer0Plugin::whitelistProcess(const std::string& processName) {
    impl_->config.whitelistedProcesses.insert(processName);
}

void Zer0Plugin::whitelistHash(const std::string& hash) {
    impl_->config.whitelistedHashes.insert(hash);
}

void Zer0Plugin::setDetectionCallback(DetectionCallback callback) {
    impl_->detectionCallback = std::move(callback);
}

bool Zer0Plugin::quarantineFile(const std::string& path) {
    auto& logger = Logger::instance();
    
    if (access(path.c_str(), F_OK) != 0) {
        return false;
    }
    
    // Extract filename from path
    const char* filename = strrchr(path.c_str(), '/');
    if (!filename) filename = path.c_str();
    else filename++;
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    std::string quarantinePath = impl_->quarantineDir + "/" + 
                                  std::to_string(timestamp) + "_" + filename;
    
    try {
        if (rename(path.c_str(), quarantinePath.c_str()) != 0) {
            return false;
        }
        
        // Save original path for restore
        std::ofstream meta(quarantinePath + ".meta");
        meta << "original_path=" << path << "\n";
        meta << "quarantine_time=" << timestamp << "\n";
        meta.close();
        
        logger.log(LogLevel::INFO, "File quarantined: " + path + " -> " + quarantinePath, "Zer0");
        
        return true;
    } catch (...) {
        return false;
    }
}

bool Zer0Plugin::restoreFile(const std::string& quarantinePath, const std::string& originalPath) {
    auto& logger = Logger::instance();

    if (access(quarantinePath.c_str(), F_OK) != 0) {
        return false;
    }

    try {
        if (access(originalPath.c_str(), F_OK) == 0) {
            logger.log(LogLevel::ERROR, "Original file already exists: " + originalPath, "Zer0");
            return false;
        }

        if (rename(quarantinePath.c_str(), originalPath.c_str()) != 0) {
            logger.log(LogLevel::ERROR, "Failed to restore file", "Zer0");
            return false;
        }

        logger.log(LogLevel::INFO, "File restored: " + quarantinePath + " -> " + originalPath, "Zer0");

        // Remove meta file
        remove((quarantinePath + ".meta").c_str());

        return true;
    } catch (const std::exception& e) {
        logger.log(LogLevel::ERROR, "Failed to restore: " + std::string(e.what()), "Zer0");
        return false;
    }
}
std::vector<std::string> Zer0Plugin::getQuarantineList() const {
    std::vector<std::string> result;
    
    struct stat st;
    if (stat(impl_->quarantineDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        return result;
    }
    
    DIR* dir = opendir(impl_->quarantineDir.c_str());
    if (!dir) {
        return result;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name != "." && name != ".." && name.find(".meta") == std::string::npos) {
            result.push_back(impl_->quarantineDir + "/" + name);
        }
    }
    
    closedir(dir);
    return result;
}

Zer0Plugin::Stats Zer0Plugin::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->statsMutex);
    // Return empty stats for now since stats member was removed
    Stats emptyStats;
    return emptyStats;
}

// YARA integration
std::vector<std::string> Zer0Plugin::scanWithYara(const std::string& path) {
    if (!impl_->yaraEngine) {
        return {};
    }
    
    auto results = impl_->yaraEngine->scanFile(path);
    std::vector<std::string> ruleNames;
    
    for (const auto& result : results) {
        ruleNames.push_back(result.ruleName);
    }
    
    return ruleNames;
}

bool Zer0Plugin::updateYaraRules() {
    if (!impl_->yaraEngine) {
        return false;
    }
    
    if (!impl_->config.yaraRulesUrl.empty()) {
        return impl_->yaraEngine->updateRules(impl_->config.yaraRulesUrl);
    }
    
    return false;
}

// ML integration
double Zer0Plugin::analyzeWithML(const std::string& path) {
    if (!impl_->mlEngine) {
        return 0.0;
    }
    
    auto features = impl_->mlEngine->extractFeatures(path);
    auto result = impl_->mlEngine->analyze(features);
    
    return result.anomalyScore;
}

bool Zer0Plugin::trainMLModel() {
    if (!impl_->mlEngine) {
        return false;
    }
    
    // Collect training data from recent events
    std::vector<TrainingPoint> trainingData;
    
    // In a real implementation, would collect from database
    // For now, return true as placeholder
    return true;
}

// Process monitoring
ProcessInfo Zer0Plugin::getProcessInfo(pid_t pid) {
    if (!impl_->processMonitor) {
        return ProcessInfo{};
    }
    
    return impl_->processMonitor->getProcessInfo(pid);
}

std::vector<DetectionResult> Zer0Plugin::analyzeProcessTree(pid_t rootPid) {
    std::vector<DetectionResult> results;
    
    if (!impl_->processMonitor) {
        return results;
    }
    
    auto tree = impl_->processMonitor->getProcessTree(rootPid);
    
    for (const auto& process : tree) {
        if (process.suspicious) {
            DetectionResult result;
            result.level = static_cast<ThreatLevel>(static_cast<int>(process.suspicious ? 0.8 : 0.1 * 5));
            result.type = Zer0::ThreatType::ANOMALOUS_BEHAVIOR;
            result.pid = process.pid;
            result.processName = process.name;
            result.confidence = process.suspicious ? 0.8 : 0.1;
            result.description = "Suspicious process detected: " + process.name;
            
            result.details["behavior"] = process.suspicious ? "suspicious" : "unknown";
            
            results.push_back(result);
        }
    }
    
    return results;
}

void Zer0Plugin::setProcessCallback(ProcessCallback callback) {
    if (impl_->processMonitor) {
        impl_->processMonitor->setProcessCallback(callback);
    }
}

// Automated response
bool Zer0Plugin::executeResponse(const DetectionResult& threat, ResponseAction action) {
    if (!impl_->responseEngine) {
        return false;
    }
    
    auto result = impl_->responseEngine->executeResponse(threat, action);
    
    // Update stats
    if (result.success) {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
    }
    
    return result.success;
}

ResponseAction Zer0Plugin::getRecommendedResponse(ThreatLevel level) {
    if (!impl_->responseEngine) {
        return Zer0::ResponseAction::LOG_ONLY;
    }
    
    DetectionResult dummyThreat;
    dummyThreat.level = level;
    
    return impl_->responseEngine->getRecommendedResponse(dummyThreat);
}

// Timeline and reporting
std::vector<DetectionResult> Zer0Plugin::getThreatTimeline(
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end) {
    std::vector<DetectionResult> timeline;
    
    // In a real implementation, would query from database
    // For now, return empty
    
    return timeline;
}

std::string Zer0Plugin::generateThreatReport(const DetectionResult& threat) {
    std::ostringstream report;
    
    report << "=== THREAT REPORT ===\n";
    report << "Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
    
    report << "Threat Details:\n";
    report << "  File: " << threat.filePath << "\n";
    report << "  Type: " << static_cast<int>(threat.type) << "\n";
    report << "  Level: " << static_cast<int>(threat.level) << "\n";
    report << "  Confidence: " << (threat.confidence * 100) << "%\n";
    report << "  Description: " << threat.description << "\n";
    
    if (!threat.processName.empty()) {
        report << "\nProcess Information:\n";
        report << "  Name: " << threat.processName << "\n";
        report << "  PID: " << threat.pid << "\n";
        report << "  Parent: " << threat.parentProcess << "\n";
    }
    
    if (!threat.matchedRules.empty()) {
        report << "\nYARA Matches:\n";
        for (const auto& rule : threat.matchedRules) {
            report << "  - " << rule << "\n";
        }
    }
    
    if (threat.anomalyScore > 0) {
        report << "\nML Analysis:\n";
        report << "  Anomaly Score: " << threat.anomalyScore << "\n";
    }
    
    report << "\nMetadata:\n";
    for (const auto& pair : threat.details) {
        report << "  " << pair.first << ": " << pair.second << "\n";
    }
    
    report << "\n=== END REPORT ===\n";
    
    return report.str();
}

// Private helper
void Zer0Plugin::handleThreat(const DetectionResult& result) {
    auto& logger = Logger::instance();
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
    }
    
    // Log threat
    std::string levelStr;
    switch (result.level) {
        case Zer0::ThreatLevel::INFO: levelStr = "INFO"; break;
        case Zer0::ThreatLevel::LOW: levelStr = "LOW"; break;
        case Zer0::ThreatLevel::MEDIUM: levelStr = "MEDIUM"; break;
        case Zer0::ThreatLevel::HIGH: levelStr = "HIGH"; break;
        case Zer0::ThreatLevel::CRITICAL: levelStr = "CRITICAL"; break;
        default: levelStr = "UNKNOWN"; break;
    }
    
    logger.log(LogLevel::WARN, "🛡️  THREAT [" + levelStr + "]: " + result.description + 
               " - " + result.filePath, "Zer0");
    
    // Save to database via storage plugin
    if (impl_->storage) {
        // Map threat type to type_id
        int threatTypeId = 0;
        switch (result.type) {
            case Zer0::ThreatType::RANSOMWARE_PATTERN: threatTypeId = 1; break;
            case Zer0::ThreatType::HIGH_ENTROPY_TEXT: threatTypeId = 2; break;
            case Zer0::ThreatType::HIDDEN_EXECUTABLE: threatTypeId = 3; break;
            case Zer0::ThreatType::EXTENSION_MISMATCH: threatTypeId = 4; break;
            case Zer0::ThreatType::DOUBLE_EXTENSION: threatTypeId = 5; break;
            case Zer0::ThreatType::MASS_MODIFICATION: threatTypeId = 6; break;
            case Zer0::ThreatType::SCRIPT_IN_DATA: threatTypeId = 7; break;
            case Zer0::ThreatType::ANOMALOUS_BEHAVIOR: threatTypeId = 8; break;
            case Zer0::ThreatType::KNOWN_MALWARE_HASH: threatTypeId = 9; break;
            case Zer0::ThreatType::SUSPICIOUS_RENAME: threatTypeId = 10; break;
            default: threatTypeId = 0; break;
        }
        
        // Map threat level to level_id
        int threatLevelId = 0;
        switch (result.level) {
            case Zer0::ThreatLevel::INFO: threatLevelId = 1; break;
            case Zer0::ThreatLevel::LOW: threatLevelId = 2; break;
            case Zer0::ThreatLevel::MEDIUM: threatLevelId = 3; break;
            case Zer0::ThreatLevel::HIGH: threatLevelId = 4; break;
            case Zer0::ThreatLevel::CRITICAL: threatLevelId = 5; break;
            default: threatLevelId = 0; break;
        }
        
        // Get file size
        long long fileSize = 0;
        try {
            struct stat st;
            if (stat(result.filePath.c_str(), &st) == 0) {
                fileSize = st.st_size;
            }
        } catch (...) {}
        
        // Save threat to database
        sqlite3* db = static_cast<sqlite3*>(impl_->storage->getDB());
        if (db) {
            // Check if threat already exists for this file (prevent duplicates)
            const char* checkSql = "SELECT id, marked_safe FROM detected_threats WHERE file_path = ?";
            sqlite3_stmt* checkStmt;
            bool exists = false;
            
            if (sqlite3_prepare_v2(db, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(checkStmt, 1, result.filePath.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(checkStmt) == SQLITE_ROW) {
                    exists = true;
                }
                sqlite3_finalize(checkStmt);
            }
            
            // Skip insertion if threat already exists for this file
            if (exists) {
                logger.log(LogLevel::DEBUG, "Threat already tracked for: " + result.filePath, "Zer0");
            } else {
                const char* sql = R"(
                    INSERT INTO detected_threats 
                    (file_path, threat_type_id, threat_level_id, threat_score, entropy, file_size, hash, quarantine_path, detected_at, marked_safe)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, datetime('now'), 0)
                )";
                
                sqlite3_stmt* stmt;
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, result.filePath.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(stmt, 2, threatTypeId);
                    sqlite3_bind_int(stmt, 3, threatLevelId);
                    sqlite3_bind_double(stmt, 4, result.confidence);
                    sqlite3_bind_null(stmt, 5);  // entropy - could be added later
                    sqlite3_bind_int64(stmt, 6, fileSize);
                    sqlite3_bind_text(stmt, 7, result.fileHash.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_null(stmt, 8);  // quarantine_path - will be updated after quarantine
                    
                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        logger.log(LogLevel::INFO, "Threat saved to database: " + result.filePath, "Zer0");
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }
    
    // Publish event
    if (impl_->eventBus) {
        impl_->eventBus->publish("THREAT_DETECTED", std::string(result.filePath));
    }
    
    // Call callback
    if (impl_->detectionCallback) {
        impl_->detectionCallback(result);
    }
    
    // Auto quarantine if enabled and threshold met
    if (impl_->config.autoQuarantine && result.level >= impl_->config.quarantineThreshold) {
        if (!quarantineFile(result.filePath)) {
            // If quarantine failed, just log it
            Logger::instance().error("Failed to quarantine file: " + result.filePath, "Zer0");
        }
    }
}

// Plugin factory

extern "C" {
    IPlugin* create_plugin() {
        return new Zer0Plugin();
    }
    
    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace SentinelFS
