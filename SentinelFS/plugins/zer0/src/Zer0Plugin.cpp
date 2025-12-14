#include "Zer0Plugin.h"
#include "MagicBytes.h"
#include "BehaviorAnalyzer.h"
#include "MLEngine.h"
#include "YaraScanner.h"
#include "ProcessMonitor.h"
#include "AutoResponse.h"
#include "EventBus.h"
#include "IStorageAPI.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <sqlite3.h>

namespace SentinelFS {

using namespace Zer0;

/**
 * @brief Implementation class (PIMPL)
 */
class Zer0Plugin::Impl {
public:
    EventBus* eventBus{nullptr};
    IStorageAPI* storage{nullptr};
    
    Zer0Config config;
    DetectionCallback detectionCallback;
    
    std::unique_ptr<BehaviorAnalyzer> behaviorAnalyzer;
    
    // New components
    std::unique_ptr<MLEngine> mlEngine;
    std::unique_ptr<YaraScanner> yaraScanner;
    std::unique_ptr<ProcessMonitor> processMonitor;
    std::unique_ptr<AutoResponse> autoResponse;
    
    Stats stats;
    mutable std::mutex statsMutex;
    
    std::string quarantineDir;
    std::string rulesDir;
    
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
        impl_->rulesDir = std::string(home) + "/.local/share/sentinelfs/zer0_rules";
    } else {
        impl_->quarantineDir = "/tmp/zer0_quarantine";
        impl_->rulesDir = "/tmp/zer0_rules";
    }
    
    // Also check for rules in plugin directory
    std::string pluginRulesDir = "/home/rei/sentinelFS_demo/SentinelFS/SentinelFS/plugins/zer0/rules";
    if (std::filesystem::exists(pluginRulesDir)) {
        impl_->rulesDir = pluginRulesDir;
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
    
    // Start behavior analyzer
    impl_->behaviorAnalyzer->start(impl_->config.behaviorWindow);
    
    // Subscribe to file events
    if (eventBus) {
        eventBus->subscribe("FILE_CREATED", [this](const std::any& data) {
            try {
                std::string path = std::any_cast<std::string>(data);
                auto result = analyzeFile(path);
                if (result.level >= ThreatLevel::MEDIUM) {
                    handleThreat(result);
                }
            } catch (...) {}
        });
        
        eventBus->subscribe("FILE_MODIFIED", [this](const std::any& data) {
            try {
                std::string path = std::any_cast<std::string>(data);
                auto result = analyzeFile(path);
                if (result.level >= ThreatLevel::MEDIUM) {
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
                if (behaviorResult.level >= ThreatLevel::MEDIUM) {
                    handleThreat(behaviorResult);
                }
            } catch (...) {}
        });
        
        // Subscribe to ML training command
        eventBus->subscribe("zer0.train_model", [this](const std::any& data) {
            try {
                std::string directory = std::any_cast<std::string>(data);
                auto& logger = Logger::instance();
                logger.log(LogLevel::INFO, "Received training request for: " + directory, "Zer0");
                
                // Run training in background thread
                std::thread([this, directory]() {
                    trainModel(directory);
                }).detach();
            } catch (const std::exception& e) {
                Logger::instance().log(LogLevel::ERROR, 
                    std::string("Training event error: ") + e.what(), "Zer0");
            }
        });
        
        // Subscribe to status request - publish current stats
        eventBus->subscribe("zer0.get_status", [this](const std::any&) {
            publishStatus();
        });
    }
    
    // Initialize YARA scanner early
    impl_->yaraScanner = std::make_unique<YaraScanner>();
    impl_->yaraScanner->initialize();
    
    // Load YARA rules
    std::string defaultRules = impl_->rulesDir + "/default.yar";
    if (std::filesystem::exists(defaultRules)) {
        if (impl_->yaraScanner->loadRulesFromFile(defaultRules)) {
            impl_->yaraScanner->compileRules();
            logger.log(LogLevel::INFO, "YARA rules loaded: " + std::to_string(impl_->yaraScanner->getRuleCount()) + " rules from " + defaultRules, "Zer0");
        }
    } else {
        logger.log(LogLevel::WARN, "YARA rules file not found: " + defaultRules, "Zer0");
    }
    
    logger.log(LogLevel::INFO, "Zer0 initialized successfully", "Zer0");
    logger.log(LogLevel::INFO, "  - Magic byte validation: enabled", "Zer0");
    logger.log(LogLevel::INFO, "  - Behavioral analysis: enabled", "Zer0");
    logger.log(LogLevel::INFO, "  - YARA scanning: " + std::string(impl_->yaraScanner ? "enabled" : "disabled"), "Zer0");
    logger.log(LogLevel::INFO, "  - File type awareness: enabled", "Zer0");
    logger.log(LogLevel::INFO, "  - Quarantine directory: " + impl_->quarantineDir, "Zer0");
    logger.log(LogLevel::INFO, "  - Rules directory: " + impl_->rulesDir, "Zer0");
    
    return true;
}

void Zer0Plugin::shutdown() {
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Shutting down Zer0", "Zer0");
    
    if (impl_->behaviorAnalyzer) {
        impl_->behaviorAnalyzer->stop();
    }
}

void Zer0Plugin::setStoragePlugin(IStorageAPI* storage) {
    impl_->storage = storage;
}

DetectionResult Zer0Plugin::analyzeFile(const std::string& path) {
    return analyzeFile(path, 0, "");
}

DetectionResult Zer0Plugin::analyzeFile(const std::string& path, pid_t pid, const std::string& processName) {
    auto& logger = Logger::instance();
    
    DetectionResult result;
    result.filePath = path;
    result.pid = pid;
    result.processName = processName;
    result.timestamp = std::chrono::steady_clock::now();
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
        impl_->stats.filesAnalyzed++;
    }
    
    // Check whitelist
    if (impl_->isWhitelisted(path, pid, processName)) {
        result.level = ThreatLevel::NONE;
        result.description = "Whitelisted";
        return result;
    }
    
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        result.level = ThreatLevel::NONE;
        result.description = "File does not exist";
        return result;
    }
    
    // Read file header
    auto header = impl_->readFileHeader(path);
    if (header.empty()) {
        result.level = ThreatLevel::NONE;
        result.description = "Could not read file";
        return result;
    }
    
    // Detect file category from content
    auto& magicBytes = MagicBytes::instance();
    FileCategory detectedCategory = magicBytes.detectCategory(header);
    result.category = detectedCategory;
    
    // Get expected category from extension
    std::string ext = impl_->getExtension(path);
    FileCategory expectedCategory = magicBytes.getCategoryForExtension(ext);
    
    // Calculate hash
    result.fileHash = impl_->calculateHash(path);
    
    // Check for known malware hash
    if (impl_->config.whitelistedHashes.count(result.fileHash) > 0) {
        result.level = ThreatLevel::NONE;
        result.description = "Known safe hash";
        return result;
    }
    
    // === THREAT DETECTION ===
    
    // 1. Check for double extension (file.pdf.exe)
    if (hasDoubleExtension(path)) {
        result.level = ThreatLevel::HIGH;
        result.type = ThreatType::DOUBLE_EXTENSION;
        result.confidence = 0.9;
        result.description = "Double extension detected (possible malware disguise)";
        result.details["pattern"] = "double_extension";
        
        logger.log(LogLevel::WARN, "⚠️  Double extension: " + path, "Zer0");
        return result;
    }
    
    // 2. Check for hidden executable
    if (expectedCategory != FileCategory::EXECUTABLE && 
        expectedCategory != FileCategory::UNKNOWN &&
        magicBytes.isExecutable(header)) {
        
        result.level = ThreatLevel::CRITICAL;
        result.type = ThreatType::HIDDEN_EXECUTABLE;
        result.confidence = 0.95;
        result.description = "Executable disguised as " + ext + " file";
        result.details["expected"] = ext;
        result.details["actual"] = "executable";
        
        logger.log(LogLevel::ERROR, "🚨 HIDDEN EXECUTABLE: " + path, "Zer0");
        return result;
    }
    
    // 3. Check for extension mismatch (non-critical cases)
    if (expectedCategory != FileCategory::UNKNOWN && 
        detectedCategory != FileCategory::UNKNOWN &&
        expectedCategory != detectedCategory) {
        
        // Skip benign mismatches:
        // - Office documents are ZIP archives (docx, xlsx, pptx, etc.)
        // - TEXT files detected as TEXT (scripts, configs, etc.)
        // - SVG files can be detected as TEXT (XML-based)
        bool isBenignMismatch = 
            (expectedCategory == FileCategory::DOCUMENT && detectedCategory == FileCategory::ARCHIVE) ||
            (expectedCategory == FileCategory::TEXT && detectedCategory == FileCategory::TEXT) ||
            (expectedCategory == FileCategory::TEXT) ||  // All text-based extensions are safe
            (expectedCategory == FileCategory::IMAGE && ext == "svg" && detectedCategory == FileCategory::TEXT);
        
        if (!isBenignMismatch) {
            result.level = ThreatLevel::MEDIUM;
            result.type = ThreatType::EXTENSION_MISMATCH;
            result.confidence = 0.7;
            result.description = "File content doesn't match extension";
            result.details["expected_type"] = std::to_string(static_cast<int>(expectedCategory));
            result.details["detected_type"] = std::to_string(static_cast<int>(detectedCategory));
            
            logger.log(LogLevel::WARN, "⚠️  Extension mismatch: " + path, "Zer0");
            return result;
        }
    }
    
    // 4. Check for embedded scripts in data files
    if (detectedCategory == FileCategory::IMAGE || 
        detectedCategory == FileCategory::DOCUMENT ||
        detectedCategory == FileCategory::ARCHIVE) {
        
        // Read more content for script detection
        auto fullContent = impl_->readFileHeader(path, 65536);
        if (magicBytes.hasEmbeddedScript(fullContent)) {
            result.level = ThreatLevel::HIGH;
            result.type = ThreatType::SCRIPT_IN_DATA;
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
            result.level = ThreatLevel::HIGH;
            result.type = ThreatType::RANSOMWARE_PATTERN;
            result.confidence = 0.9;
            result.description = "Ransomware file extension detected: ." + ext;
            result.details["extension"] = ext;
            
            logger.log(LogLevel::WARN, "🔐 Ransomware extension: " + path, "Zer0");
            return result;
        }
    }
    
    // 6. Entropy analysis (only for text/config/unknown files)
    if (detectedCategory == FileCategory::TEXT ||
        detectedCategory == FileCategory::CONFIG ||
        detectedCategory == FileCategory::UNKNOWN) {
        
        double entropy = impl_->calculateEntropy(header);
        double threshold = impl_->getEntropyThreshold(detectedCategory);
        
        if (entropy > threshold) {
            result.level = ThreatLevel::MEDIUM;
            result.type = ThreatType::HIGH_ENTROPY_TEXT;
            result.confidence = std::min(1.0, (entropy - threshold) / 2.0 + 0.5);
            result.description = "High entropy in text file (possibly encrypted/obfuscated)";
            result.details["entropy"] = std::to_string(entropy);
            result.details["threshold"] = std::to_string(threshold);
            
            logger.log(LogLevel::WARN, "⚠️  High entropy text: " + path + 
                       " (entropy: " + std::to_string(entropy) + ")", "Zer0");
            return result;
        }
    }
    
    // 7. YARA scanning for malware signatures
    if (impl_->yaraScanner && impl_->yaraScanner->getRuleCount() > 0) {
        auto yaraResult = impl_->yaraScanner->scanFile(path);
        if (!yaraResult.matches.empty()) {
            // Find the highest severity match
            std::string highestSeverity = "low";
            std::string matchedRules;
            std::string matchedCategory;
            
            for (const auto& match : yaraResult.matches) {
                if (!matchedRules.empty()) matchedRules += ", ";
                matchedRules += match.ruleName;
                
                if (match.severity == "critical") {
                    highestSeverity = "critical";
                    matchedCategory = match.category;
                } else if (match.severity == "high" && highestSeverity != "critical") {
                    highestSeverity = "high";
                    matchedCategory = match.category;
                } else if (match.severity == "medium" && highestSeverity != "critical" && highestSeverity != "high") {
                    highestSeverity = "medium";
                    matchedCategory = match.category;
                }
            }
            
            // Set threat level based on YARA match severity
            if (highestSeverity == "critical") {
                result.level = ThreatLevel::CRITICAL;
                result.type = ThreatType::KNOWN_MALWARE_HASH;
            } else if (highestSeverity == "high") {
                result.level = ThreatLevel::HIGH;
                result.type = ThreatType::KNOWN_MALWARE_HASH;
            } else if (highestSeverity == "medium") {
                result.level = ThreatLevel::MEDIUM;
                result.type = ThreatType::ANOMALOUS_BEHAVIOR;
            } else {
                result.level = ThreatLevel::LOW;
                result.type = ThreatType::ANOMALOUS_BEHAVIOR;
            }
            
            result.confidence = 0.9;
            result.description = "YARA match: " + matchedRules;
            result.details["yara_rules"] = matchedRules;
            result.details["yara_category"] = matchedCategory;
            result.details["yara_severity"] = highestSeverity;
            
            logger.log(LogLevel::WARN, "🔍 YARA match [" + highestSeverity + "]: " + path + " (" + matchedRules + ")", "Zer0");
            return result;
        }
    }
    
    // 8. Record event for behavioral analysis
    BehaviorEvent event;
    event.path = path;
    event.eventType = "MODIFY";
    event.pid = pid;
    event.processName = processName;
    event.timestamp = std::chrono::steady_clock::now();
    impl_->behaviorAnalyzer->recordEvent(event);
    
    // No threat detected
    result.level = ThreatLevel::NONE;
    result.type = ThreatType::NONE;
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
        result.type = ThreatType::RANSOMWARE_PATTERN;
    } else if (analysis.pattern == BehaviorPattern::SINGLE_PROCESS_STORM) {
        result.type = ThreatType::ANOMALOUS_BEHAVIOR;
        result.pid = analysis.suspiciousPid;
        result.processName = analysis.suspiciousProcess;
    } else if (analysis.pattern == BehaviorPattern::MASS_DELETION) {
        result.type = ThreatType::MASS_MODIFICATION;
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
    std::transform(lastExt.begin(), lastExt.end(), lastExt.begin(), ::tolower);
    
    // Check if last extension is executable
    if (execExtensions.count(lastExt) > 0) {
        // Get second-to-last extension
        std::string prevExt = filename.substr(dots[dots.size() - 2] + 1, 
                                               dots.back() - dots[dots.size() - 2] - 1);
        std::transform(prevExt.begin(), prevExt.end(), prevExt.begin(), ::tolower);
        
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
    
    if (!std::filesystem::exists(path)) {
        return false;
    }
    
    std::string filename = std::filesystem::path(path).filename().string();
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    std::string quarantinePath = impl_->quarantineDir + "/" + 
                                  std::to_string(timestamp) + "_" + filename;
    
    try {
        std::filesystem::rename(path, quarantinePath);
        
        // Save original path for restore
        std::ofstream meta(quarantinePath + ".meta");
        meta << "original_path=" << path << "\n";
        meta << "quarantine_time=" << timestamp << "\n";
        meta.close();
        
        logger.log(LogLevel::INFO, "File quarantined: " + path + " -> " + quarantinePath, "Zer0");
        
        {
            std::lock_guard<std::mutex> lock(impl_->statsMutex);
            impl_->stats.filesQuarantined++;
        }
        
        return true;
    } catch (const std::exception& e) {
        logger.log(LogLevel::ERROR, "Failed to quarantine: " + std::string(e.what()), "Zer0");
        return false;
    }
}

bool Zer0Plugin::restoreFile(const std::string& quarantinePath, const std::string& originalPath) {
    auto& logger = Logger::instance();
    
    if (!std::filesystem::exists(quarantinePath)) {
        return false;
    }
    
    try {
        std::filesystem::rename(quarantinePath, originalPath);
        
        // Remove meta file
        std::filesystem::remove(quarantinePath + ".meta");
        
        logger.log(LogLevel::INFO, "File restored: " + quarantinePath + " -> " + originalPath, "Zer0");
        return true;
    } catch (const std::exception& e) {
        logger.log(LogLevel::ERROR, "Failed to restore: " + std::string(e.what()), "Zer0");
        return false;
    }
}

std::vector<std::string> Zer0Plugin::getQuarantineList() const {
    std::vector<std::string> result;
    
    if (!std::filesystem::exists(impl_->quarantineDir)) {
        return result;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(impl_->quarantineDir)) {
        if (entry.path().extension() != ".meta") {
            result.push_back(entry.path().string());
        }
    }
    
    return result;
}

Zer0Plugin::Stats Zer0Plugin::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->statsMutex);
    return impl_->stats;
}

// Private helper
void Zer0Plugin::handleThreat(const DetectionResult& result) {
    auto& logger = Logger::instance();
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
        impl_->stats.threatsDetected++;
        impl_->stats.threatsByType[result.type]++;
        impl_->stats.threatsByLevel[result.level]++;
    }
    
    // Log threat
    std::string levelStr;
    switch (result.level) {
        case ThreatLevel::INFO: levelStr = "INFO"; break;
        case ThreatLevel::LOW: levelStr = "LOW"; break;
        case ThreatLevel::MEDIUM: levelStr = "MEDIUM"; break;
        case ThreatLevel::HIGH: levelStr = "HIGH"; break;
        case ThreatLevel::CRITICAL: levelStr = "CRITICAL"; break;
        default: levelStr = "UNKNOWN"; break;
    }
    
    logger.log(LogLevel::WARN, "🛡️  THREAT [" + levelStr + "]: " + result.description + 
               " - " + result.filePath, "Zer0");
    
    // Save to database via storage plugin
    if (impl_->storage) {
        // Map threat type to type_id
        int threatTypeId = 0;
        switch (result.type) {
            case ThreatType::RANSOMWARE_PATTERN: threatTypeId = 1; break;
            case ThreatType::HIGH_ENTROPY_TEXT: threatTypeId = 2; break;
            case ThreatType::HIDDEN_EXECUTABLE: threatTypeId = 3; break;
            case ThreatType::EXTENSION_MISMATCH: threatTypeId = 4; break;
            case ThreatType::DOUBLE_EXTENSION: threatTypeId = 5; break;
            case ThreatType::MASS_MODIFICATION: threatTypeId = 6; break;
            case ThreatType::SCRIPT_IN_DATA: threatTypeId = 7; break;
            case ThreatType::ANOMALOUS_BEHAVIOR: threatTypeId = 8; break;
            case ThreatType::KNOWN_MALWARE_HASH: threatTypeId = 9; break;
            case ThreatType::SUSPICIOUS_RENAME: threatTypeId = 10; break;
            default: threatTypeId = 0; break;
        }
        
        // Map threat level to level_id
        int threatLevelId = 0;
        switch (result.level) {
            case ThreatLevel::INFO: threatLevelId = 1; break;
            case ThreatLevel::LOW: threatLevelId = 2; break;
            case ThreatLevel::MEDIUM: threatLevelId = 3; break;
            case ThreatLevel::HIGH: threatLevelId = 4; break;
            case ThreatLevel::CRITICAL: threatLevelId = 5; break;
            default: threatLevelId = 0; break;
        }
        
        // Get file size
        long long fileSize = 0;
        try {
            if (std::filesystem::exists(result.filePath)) {
                fileSize = std::filesystem::file_size(result.filePath);
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
        impl_->eventBus->publish("THREAT_DETECTED", result.filePath);
    }
    
    // Call callback
    if (impl_->detectionCallback) {
        impl_->detectionCallback(result);
    }
    
    // Auto quarantine if enabled and threshold met
    if (impl_->config.autoQuarantine && result.level >= impl_->config.quarantineThreshold) {
        quarantineFile(result.filePath);
    }
}

// New component accessors

Zer0::MLEngine* Zer0Plugin::getMLEngine() {
    return impl_->mlEngine.get();
}

Zer0::YaraScanner* Zer0Plugin::getYaraScanner() {
    return impl_->yaraScanner.get();
}

Zer0::ProcessMonitor* Zer0Plugin::getProcessMonitor() {
    return impl_->processMonitor.get();
}

Zer0::AutoResponse* Zer0Plugin::getAutoResponse() {
    return impl_->autoResponse.get();
}

Zer0::DetectionResult Zer0Plugin::comprehensiveScan(const std::string& path) {
    auto& logger = Logger::instance();
    
    // Start with basic file analysis
    DetectionResult result = analyzeFile(path);
    
    // YARA scan
    if (impl_->yaraScanner) {
        auto yaraResult = impl_->yaraScanner->scanFile(path);
        if (yaraResult.hasMatches()) {
            // Upgrade threat level if YARA found something
            if (yaraResult.hasCritical()) {
                result.level = ThreatLevel::CRITICAL;
                result.type = ThreatType::KNOWN_MALWARE_HASH;
            } else if (yaraResult.hasHigh() && result.level < ThreatLevel::HIGH) {
                result.level = ThreatLevel::HIGH;
            }
            
            // Add YARA match details
            for (const auto& match : yaraResult.matches) {
                result.details["yara_rule_" + match.ruleName] = match.ruleDescription;
            }
            
            logger.log(LogLevel::WARN, "YARA match found: " + path, "Zer0");
        }
    }
    
    // ML analysis
    if (impl_->mlEngine) {
        auto features = MLEngine::extractFileFeatures(path);
        auto anomalyResult = impl_->mlEngine->analyzeFeatures(features);
        
        if (anomalyResult.anomalyScore > 0.7) {
            // ML detected anomaly
            if (result.level < ThreatLevel::MEDIUM) {
                result.level = ThreatLevel::MEDIUM;
            }
            result.details["ml_anomaly_score"] = std::to_string(anomalyResult.anomalyScore);
            result.details["ml_category"] = anomalyResult.category;
            
            logger.log(LogLevel::WARN, "ML anomaly detected: " + path + 
                      " (score: " + std::to_string(anomalyResult.anomalyScore) + ")", "Zer0");
        }
        
        // Update baseline with this file
        impl_->mlEngine->updateBaseline(features);
    }
    
    // Process auto-response if threat detected
    if (impl_->autoResponse && result.level >= ThreatLevel::MEDIUM) {
        impl_->autoResponse->processDetection(result);
    }
    
    return result;
}

bool Zer0Plugin::startMonitoring() {
    auto& logger = Logger::instance();
    
    // Initialize components if not already done
    if (!impl_->mlEngine) {
        impl_->mlEngine = std::make_unique<MLEngine>();
        impl_->mlEngine->initialize();
        logger.log(LogLevel::INFO, "ML Engine initialized", "Zer0");
    }
    
    if (!impl_->yaraScanner) {
        impl_->yaraScanner = std::make_unique<YaraScanner>();
        impl_->yaraScanner->initialize();
        
        // Load default rules
        std::string rulesPath = impl_->rulesDir + "/default.yar";
        if (std::filesystem::exists(rulesPath)) {
            impl_->yaraScanner->loadRulesFromFile(rulesPath);
            impl_->yaraScanner->compileRules();
            logger.log(LogLevel::INFO, "YARA rules loaded from: " + rulesPath, "Zer0");
        }
    }
    
    if (!impl_->processMonitor) {
        impl_->processMonitor = std::make_unique<ProcessMonitor>();
        ProcessMonitorConfig pmConfig;
        impl_->processMonitor->initialize(pmConfig);
        
        // Set up callbacks
        impl_->processMonitor->setSuspiciousBehaviorCallback(
            [this](const SuspiciousBehavior& behavior) {
                if (impl_->autoResponse) {
                    impl_->autoResponse->processBehavior(behavior);
                }
            }
        );
    }
    
    if (!impl_->autoResponse) {
        impl_->autoResponse = std::make_unique<AutoResponse>();
        AutoResponseConfig arConfig;
        arConfig.quarantineDir = impl_->quarantineDir;
        impl_->autoResponse->initialize(arConfig);
        logger.log(LogLevel::INFO, "Auto Response system initialized", "Zer0");
    }
    
    // Start process monitoring
    if (impl_->processMonitor) {
        impl_->processMonitor->start();
        logger.log(LogLevel::INFO, "Process monitoring started", "Zer0");
    }
    
    return true;
}

void Zer0Plugin::stopMonitoring() {
    auto& logger = Logger::instance();
    
    if (impl_->processMonitor) {
        impl_->processMonitor->stop();
        logger.log(LogLevel::INFO, "Process monitoring stopped", "Zer0");
    }
}

bool Zer0Plugin::isMonitoring() const {
    return impl_->processMonitor && impl_->processMonitor->isRunning();
}

int Zer0Plugin::trainModel(const std::string& directoryPath) {
    auto& logger = Logger::instance();
    
    // Initialize ML engine if needed
    if (!impl_->mlEngine) {
        impl_->mlEngine = std::make_unique<MLEngine>();
        impl_->mlEngine->initialize();
        logger.log(LogLevel::INFO, "ML Engine initialized for training", "Zer0");
    }
    
    logger.log(LogLevel::INFO, "Starting ML model training from: " + directoryPath, "Zer0");
    
    int filesProcessed = impl_->mlEngine->trainFromDirectory(
        directoryPath,
        true,  // recursive
        [&logger](int current, int total, const std::string& file) {
            if (current % 100 == 0 || current == total) {
                logger.log(LogLevel::DEBUG, 
                    "Training progress: " + std::to_string(current) + "/" + std::to_string(total), 
                    "Zer0");
            }
        }
    );
    
    logger.log(LogLevel::INFO, 
        "ML model training complete. Processed " + std::to_string(filesProcessed) + " files", 
        "Zer0");
    
    // Save model after training
    std::string modelPath = impl_->quarantineDir + "/../zer0_model";
    if (impl_->mlEngine->saveModel(modelPath)) {
        logger.log(LogLevel::INFO, "ML model saved to: " + modelPath, "Zer0");
    }
    
    return filesProcessed;
}

Zer0::MLEngine::TrainingStatus Zer0Plugin::getTrainingStatus() const {
    if (impl_->mlEngine) {
        return impl_->mlEngine->getTrainingStatus();
    }
    return Zer0::MLEngine::TrainingStatus{};
}

void Zer0Plugin::publishStatus() {
    if (!impl_->eventBus) return;
    
    // Build status JSON
    std::ostringstream ss;
    ss << "{";
    ss << "\"enabled\":true,";
    
    // Basic stats
    {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
        ss << "\"filesAnalyzed\":" << impl_->stats.filesAnalyzed << ",";
        ss << "\"threatsDetected\":" << impl_->stats.threatsDetected << ",";
        ss << "\"filesQuarantined\":" << impl_->stats.filesQuarantined << ",";
    }
    
    // ML stats
    if (impl_->mlEngine) {
        auto mlStats = impl_->mlEngine->getStats();
        ss << "\"mlStats\":{";
        ss << "\"samplesProcessed\":" << mlStats.samplesProcessed << ",";
        ss << "\"anomaliesDetected\":" << mlStats.anomaliesDetected << ",";
        ss << "\"avgAnomalyScore\":" << mlStats.avgAnomalyScore << ",";
        ss << "\"modelLoaded\":" << (mlStats.samplesProcessed > 0 ? "true" : "false");
        ss << "},";
    }
    
    // YARA stats
    if (impl_->yaraScanner) {
        auto yaraStats = impl_->yaraScanner->getStats();
        ss << "\"yaraStats\":{";
        ss << "\"rulesLoaded\":" << impl_->yaraScanner->getRuleCount() << ",";
        ss << "\"filesScanned\":" << yaraStats.filesScanned << ",";
        ss << "\"matchesFound\":" << yaraStats.matchesFound;
        ss << "},";
    }
    
    // Process monitor stats
    if (impl_->processMonitor) {
        auto pmStats = impl_->processMonitor->getStats();
        ss << "\"processMonitorStats\":{";
        ss << "\"processesTracked\":" << pmStats.processesTracked << ",";
        ss << "\"suspiciousDetected\":" << pmStats.suspiciousDetected << ",";
        ss << "\"isRunning\":" << (impl_->processMonitor->isRunning() ? "true" : "false");
        ss << "},";
    }
    
    // Auto response stats
    if (impl_->autoResponse) {
        auto arStats = impl_->autoResponse->getStats();
        ss << "\"autoResponseStats\":{";
        ss << "\"actionsExecuted\":" << arStats.actionsExecuted << ",";
        ss << "\"filesQuarantined\":" << arStats.filesQuarantined << ",";
        ss << "\"alertsSent\":" << arStats.alertsSent;
        ss << "},";
    }
    
    ss << "\"threatLevel\":\"NONE\"";
    ss << "}";
    
    impl_->eventBus->publish("zer0.status", ss.str());
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
