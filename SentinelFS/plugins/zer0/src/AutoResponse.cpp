#include "AutoResponse.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <signal.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <uuid/uuid.h>
#include <cstring>
#include <cerrno>

namespace SentinelFS {
namespace Zer0 {

// ============================================================================
// Helper functions
// ============================================================================

static std::string generateUUID() {
    uuid_t uuid;
    uuid_generate(uuid);
    char str[37];
    uuid_unparse_lower(uuid, str);
    return std::string(str);
}

static std::string calculateSHA256(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    
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

// ============================================================================
// AutoResponse Implementation
// ============================================================================

class AutoResponse::Impl {
public:
    AutoResponseConfig config;
    std::vector<ResponseRule> rules;
    std::vector<Alert> alerts;
    std::map<std::string, QuarantineInfo> quarantineMap;
    
    AlertCallback alertCallback;
    ResponseCallback responseCallback;
    ConfirmationCallback confirmationCallback;
    
    Stats stats;
    mutable std::mutex mutex;
    
    // Rate limiting
    std::map<std::string, std::chrono::steady_clock::time_point> lastActionTime;
    int actionsThisMinute{0};
    std::chrono::steady_clock::time_point minuteStart;
    
    bool checkRateLimit() {
        auto now = std::chrono::steady_clock::now();
        if (now - minuteStart > std::chrono::minutes(1)) {
            minuteStart = now;
            actionsThisMinute = 0;
        }
        
        if (actionsThisMinute >= config.maxActionsPerMinute) {
            return false;
        }
        
        actionsThisMinute++;
        return true;
    }
    
    bool isWhitelisted(const std::string& path, const std::string& process = "", pid_t pid = 0) {
        if (config.whitelistedPaths.count(path)) return true;
        if (!process.empty() && config.whitelistedProcesses.count(process)) return true;
        if (pid > 0 && config.whitelistedPids.count(pid)) return true;
        
        // Check path prefixes
        for (const auto& wp : config.whitelistedPaths) {
            if (path.find(wp) == 0) return true;
        }
        
        return false;
    }
    
    void sendAlert(const std::string& title, const std::string& message, 
                   ThreatLevel severity, const std::string& source) {
        Alert alert;
        alert.id = generateUUID();
        alert.title = title;
        alert.message = message;
        alert.severity = severity;
        alert.timestamp = std::chrono::steady_clock::now();
        alert.source = source;
        
        alerts.push_back(alert);
        stats.alertsSent++;
        
        if (alertCallback) {
            alertCallback(alert);
        }
    }
    
    ResponseResult executeAction(ResponseAction action, const std::string& target,
                                  const DetectionResult& detection) {
        ResponseResult result;
        result.action = action;
        result.target = target;
        result.timestamp = std::chrono::steady_clock::now();
        
        if (!checkRateLimit()) {
            result.success = false;
            result.message = "Rate limit exceeded";
            return result;
        }
        
        switch (action) {
            case ResponseAction::LOG:
                result.success = true;
                result.message = "Event logged";
                break;
                
            case ResponseAction::ALERT:
                sendAlert("Threat Detected", detection.description, 
                         detection.level, detection.filePath);
                result.success = true;
                result.message = "Alert sent";
                break;
                
            case ResponseAction::QUARANTINE_FILE:
                result = quarantineFileImpl(target, detection);
                break;
                
            case ResponseAction::DELETE_FILE:
                result = deleteFileImpl(target);
                break;
                
            case ResponseAction::TERMINATE_PROCESS:
                result = terminateProcessImpl(detection.pid);
                break;
                
            case ResponseAction::SUSPEND_PROCESS:
                result = suspendProcessImpl(detection.pid);
                break;
                
            case ResponseAction::BACKUP_FILE:
                result = backupFileImpl(target);
                break;
                
            default:
                result.success = false;
                result.message = "Unknown action";
                break;
        }
        
        stats.actionsExecuted++;
        
        if (responseCallback) {
            responseCallback(result);
        }
        
        return result;
    }
    
    ResponseResult quarantineFileImpl(const std::string& path, const DetectionResult& detection) {
        ResponseResult result;
        result.action = ResponseAction::QUARANTINE_FILE;
        result.target = path;
        result.timestamp = std::chrono::steady_clock::now();
        
        if (!std::filesystem::exists(path)) {
            result.success = false;
            result.message = "File does not exist";
            return result;
        }
        
        // Check quarantine size limit
        if (getQuarantineSizeImpl() >= static_cast<uint64_t>(config.maxQuarantineSize)) {
            result.success = false;
            result.message = "Quarantine size limit reached";
            return result;
        }
        
        // Create quarantine directory if needed
        std::filesystem::create_directories(config.quarantineDir);
        
        // Generate quarantine path
        std::string hash = calculateSHA256(path);
        std::string filename = std::filesystem::path(path).filename().string();
        std::string quarantinePath = config.quarantineDir + "/" + hash + "_" + filename + ".quarantine";
        
        try {
            // Move file to quarantine
            std::filesystem::rename(path, quarantinePath);
            
            // Store quarantine info
            QuarantineInfo info;
            info.originalPath = path;
            info.quarantinePath = quarantinePath;
            info.quarantineTime = std::chrono::steady_clock::now();
            info.reason = detection.description;
            info.threatLevel = detection.level;
            info.hash = hash;
            
            quarantineMap[quarantinePath] = info;
            
            // Write metadata file
            std::ofstream metaFile(quarantinePath + ".meta");
            if (metaFile) {
                metaFile << "original_path=" << path << "\n";
                metaFile << "quarantine_time=" << std::chrono::duration_cast<std::chrono::seconds>(
                    info.quarantineTime.time_since_epoch()).count() << "\n";
                metaFile << "reason=" << detection.description << "\n";
                metaFile << "threat_level=" << static_cast<int>(detection.level) << "\n";
                metaFile << "hash=" << hash << "\n";
            }
            
            result.success = true;
            result.message = "File quarantined successfully";
            result.details["quarantine_path"] = quarantinePath;
            result.details["hash"] = hash;
            
            stats.filesQuarantined++;
            
        } catch (const std::exception& e) {
            result.success = false;
            result.message = std::string("Failed to quarantine: ") + e.what();
        }
        
        return result;
    }
    
    ResponseResult deleteFileImpl(const std::string& path) {
        ResponseResult result;
        result.action = ResponseAction::DELETE_FILE;
        result.target = path;
        result.timestamp = std::chrono::steady_clock::now();
        
        if (!std::filesystem::exists(path)) {
            result.success = false;
            result.message = "File does not exist";
            return result;
        }
        
        try {
            // Create backup first if configured
            if (config.createBackups) {
                backupFileImpl(path);
            }
            
            std::filesystem::remove(path);
            result.success = true;
            result.message = "File deleted successfully";
            stats.filesDeleted++;
            
        } catch (const std::exception& e) {
            result.success = false;
            result.message = std::string("Failed to delete: ") + e.what();
        }
        
        return result;
    }
    
    ResponseResult terminateProcessImpl(pid_t pid) {
        ResponseResult result;
        result.action = ResponseAction::TERMINATE_PROCESS;
        result.target = std::to_string(pid);
        result.timestamp = std::chrono::steady_clock::now();
        
        if (pid <= 0) {
            result.success = false;
            result.message = "Invalid PID";
            return result;
        }
        
#ifdef __linux__
        if (kill(pid, SIGKILL) == 0) {
            result.success = true;
            result.message = "Process terminated";
            stats.processesTerminated++;
        } else {
            result.success = false;
            result.message = "Failed to terminate process: " + std::string(strerror(errno));
        }
#else
        result.success = false;
        result.message = "Process termination not supported on this platform";
#endif
        
        return result;
    }
    
    ResponseResult suspendProcessImpl(pid_t pid) {
        ResponseResult result;
        result.action = ResponseAction::SUSPEND_PROCESS;
        result.target = std::to_string(pid);
        result.timestamp = std::chrono::steady_clock::now();
        
        if (pid <= 0) {
            result.success = false;
            result.message = "Invalid PID";
            return result;
        }
        
#ifdef __linux__
        if (kill(pid, SIGSTOP) == 0) {
            result.success = true;
            result.message = "Process suspended";
        } else {
            result.success = false;
            result.message = "Failed to suspend process: " + std::string(strerror(errno));
        }
#else
        result.success = false;
        result.message = "Process suspension not supported on this platform";
#endif
        
        return result;
    }
    
    ResponseResult backupFileImpl(const std::string& path) {
        ResponseResult result;
        result.action = ResponseAction::BACKUP_FILE;
        result.target = path;
        result.timestamp = std::chrono::steady_clock::now();
        
        if (!std::filesystem::exists(path)) {
            result.success = false;
            result.message = "File does not exist";
            return result;
        }
        
        try {
            std::filesystem::create_directories(config.backupDir);
            
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::ostringstream oss;
            oss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
            
            std::string filename = std::filesystem::path(path).filename().string();
            std::string backupPath = config.backupDir + "/" + filename + "." + oss.str() + ".backup";
            
            std::filesystem::copy_file(path, backupPath);
            
            result.success = true;
            result.message = "Backup created";
            result.details["backup_path"] = backupPath;
            stats.backupsCreated++;
            
        } catch (const std::exception& e) {
            result.success = false;
            result.message = std::string("Failed to create backup: ") + e.what();
        }
        
        return result;
    }
    
    uint64_t getQuarantineSizeImpl() const {
        uint64_t totalSize = 0;
        
        if (std::filesystem::exists(config.quarantineDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(config.quarantineDir)) {
                if (entry.is_regular_file()) {
                    totalSize += entry.file_size();
                }
            }
        }
        
        return totalSize;
    }
};

AutoResponse::AutoResponse() : impl_(std::make_unique<Impl>()) {}

AutoResponse::~AutoResponse() = default;

bool AutoResponse::initialize(const AutoResponseConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    impl_->config = config;
    impl_->stats.startTime = std::chrono::steady_clock::now();
    impl_->minuteStart = std::chrono::steady_clock::now();
    
    // Set default paths if not specified
    if (impl_->config.quarantineDir.empty()) {
        const char* home = std::getenv("HOME");
        if (home) {
            impl_->config.quarantineDir = std::string(home) + "/.local/share/sentinelfs/zer0_quarantine";
        } else {
            impl_->config.quarantineDir = "/tmp/zer0_quarantine";
        }
    }
    
    if (impl_->config.backupDir.empty()) {
        const char* home = std::getenv("HOME");
        if (home) {
            impl_->config.backupDir = std::string(home) + "/.local/share/sentinelfs/zer0_backups";
        } else {
            impl_->config.backupDir = "/tmp/zer0_backups";
        }
    }
    
    // Create directories
    std::filesystem::create_directories(impl_->config.quarantineDir);
    std::filesystem::create_directories(impl_->config.backupDir);
    
    // Load existing quarantine metadata
    if (std::filesystem::exists(impl_->config.quarantineDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(impl_->config.quarantineDir)) {
            if (entry.path().extension() == ".meta") {
                std::ifstream metaFile(entry.path());
                if (metaFile) {
                    QuarantineInfo info;
                    std::string line;
                    while (std::getline(metaFile, line)) {
                        size_t eq = line.find('=');
                        if (eq != std::string::npos) {
                            std::string key = line.substr(0, eq);
                            std::string value = line.substr(eq + 1);
                            
                            if (key == "original_path") info.originalPath = value;
                            else if (key == "hash") info.hash = value;
                            else if (key == "reason") info.reason = value;
                        }
                    }
                    
                    std::string quarantinePath = entry.path().string();
                    quarantinePath = quarantinePath.substr(0, quarantinePath.length() - 5); // Remove .meta
                    info.quarantinePath = quarantinePath;
                    
                    impl_->quarantineMap[quarantinePath] = info;
                }
            }
        }
    }
    
    // Add default rules
    for (const auto& rule : DefaultRules::getAllDefaultRules()) {
        impl_->rules.push_back(rule);
    }
    
    return true;
}

void AutoResponse::shutdown() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->rules.clear();
    impl_->alerts.clear();
}

std::vector<ResponseResult> AutoResponse::processDetection(const DetectionResult& detection) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    std::vector<ResponseResult> results;
    impl_->stats.detectionsProcessed++;
    
    if (!impl_->config.enabled) return results;
    
    // Check whitelist
    if (impl_->isWhitelisted(detection.filePath, detection.processName, detection.pid)) {
        return results;
    }
    
    // Find matching rules
    std::vector<ResponseRule> matchingRules;
    for (const auto& rule : impl_->rules) {
        if (!rule.enabled) continue;
        
        // Check threat level
        if (detection.level < rule.minThreatLevel) continue;
        
        // Check threat type
        if (!rule.threatTypes.empty() && rule.threatTypes.find(detection.type) == rule.threatTypes.end()) {
            continue;
        }
        
        // Check confidence
        if (detection.confidence < rule.minConfidence) continue;
        
        // Check cooldown
        auto lastTime = impl_->lastActionTime.find(rule.id);
        if (lastTime != impl_->lastActionTime.end()) {
            auto elapsed = std::chrono::steady_clock::now() - lastTime->second;
            if (elapsed < rule.cooldown) continue;
        }
        
        matchingRules.push_back(rule);
    }
    
    // Sort by priority
    std::sort(matchingRules.begin(), matchingRules.end(),
              [](const ResponseRule& a, const ResponseRule& b) {
                  return a.priority > b.priority;
              });
    
    // Execute actions
    for (const auto& rule : matchingRules) {
        // Check confirmation if required
        if (rule.requireConfirmation && impl_->confirmationCallback) {
            if (!impl_->confirmationCallback(rule, detection)) {
                continue;
            }
        }
        
        for (const auto& action : rule.actions) {
            auto result = impl_->executeAction(action, detection.filePath, detection);
            results.push_back(result);
        }
        
        // Custom handler
        if (rule.customHandler) {
            results.push_back(rule.customHandler(detection));
        }
        
        impl_->lastActionTime[rule.id] = std::chrono::steady_clock::now();
    }
    
    return results;
}

std::vector<ResponseResult> AutoResponse::processBehavior(const SuspiciousBehavior& behavior) {
    // Convert to DetectionResult and process
    DetectionResult detection;
    detection.level = behavior.severity > 0.8 ? ThreatLevel::CRITICAL :
                      behavior.severity > 0.6 ? ThreatLevel::HIGH :
                      behavior.severity > 0.4 ? ThreatLevel::MEDIUM : ThreatLevel::LOW;
    detection.type = ThreatType::ANOMALOUS_BEHAVIOR;
    detection.description = behavior.description;
    detection.filePath = behavior.process.path;
    detection.pid = behavior.process.pid;
    detection.processName = behavior.process.name;
    detection.confidence = behavior.severity;
    
    return processDetection(detection);
}

std::vector<ResponseResult> AutoResponse::processYaraMatch(const YaraScanResult& result) {
    std::vector<ResponseResult> results;
    
    for (const auto& match : result.matches) {
        DetectionResult detection;
        detection.level = match.severity == "critical" ? ThreatLevel::CRITICAL :
                          match.severity == "high" ? ThreatLevel::HIGH :
                          match.severity == "medium" ? ThreatLevel::MEDIUM : ThreatLevel::LOW;
        detection.type = ThreatType::KNOWN_MALWARE_HASH;
        detection.description = match.ruleDescription;
        detection.filePath = result.filePath;
        detection.confidence = 0.95;
        
        auto r = processDetection(detection);
        results.insert(results.end(), r.begin(), r.end());
    }
    
    return results;
}

std::vector<ResponseResult> AutoResponse::processAnomaly(const AnomalyResult& anomaly) {
    DetectionResult detection;
    detection.level = anomaly.anomalyScore > 0.9 ? ThreatLevel::CRITICAL :
                      anomaly.anomalyScore > 0.7 ? ThreatLevel::HIGH :
                      anomaly.anomalyScore > 0.5 ? ThreatLevel::MEDIUM : ThreatLevel::LOW;
    detection.type = ThreatType::ANOMALOUS_BEHAVIOR;
    detection.description = anomaly.category;
    detection.confidence = anomaly.confidence;
    
    for (const auto& reason : anomaly.reasons) {
        detection.details["reason_" + std::to_string(detection.details.size())] = reason;
    }
    
    return processDetection(detection);
}

ResponseResult AutoResponse::quarantineFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    DetectionResult detection;
    detection.description = "Manual quarantine";
    detection.level = ThreatLevel::HIGH;
    
    return impl_->quarantineFileImpl(path, detection);
}

ResponseResult AutoResponse::restoreFile(const std::string& quarantinePath) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    ResponseResult result;
    result.action = ResponseAction::RESTORE_FILE;
    result.target = quarantinePath;
    result.timestamp = std::chrono::steady_clock::now();
    
    auto it = impl_->quarantineMap.find(quarantinePath);
    if (it == impl_->quarantineMap.end()) {
        result.success = false;
        result.message = "Quarantine info not found";
        return result;
    }
    
    try {
        std::filesystem::rename(quarantinePath, it->second.originalPath);
        std::filesystem::remove(quarantinePath + ".meta");
        impl_->quarantineMap.erase(it);
        
        result.success = true;
        result.message = "File restored successfully";
        result.details["original_path"] = it->second.originalPath;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.message = std::string("Failed to restore: ") + e.what();
    }
    
    return result;
}

ResponseResult AutoResponse::deleteFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->deleteFileImpl(path);
}

ResponseResult AutoResponse::terminateProcess(pid_t pid) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->terminateProcessImpl(pid);
}

ResponseResult AutoResponse::suspendProcess(pid_t pid) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->suspendProcessImpl(pid);
}

ResponseResult AutoResponse::resumeProcess(pid_t pid) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    ResponseResult result;
    result.action = ResponseAction::NONE;
    result.target = std::to_string(pid);
    result.timestamp = std::chrono::steady_clock::now();
    
#ifdef __linux__
    if (kill(pid, SIGCONT) == 0) {
        result.success = true;
        result.message = "Process resumed";
    } else {
        result.success = false;
        result.message = "Failed to resume process";
    }
#else
    result.success = false;
    result.message = "Not supported on this platform";
#endif
    
    return result;
}

ResponseResult AutoResponse::isolateProcess(pid_t pid) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    ResponseResult result;
    result.action = ResponseAction::ISOLATE_NETWORK;
    result.target = std::to_string(pid);
    result.timestamp = std::chrono::steady_clock::now();
    
    // Network isolation would require iptables/nftables or cgroups
    // This is a placeholder implementation
    result.success = false;
    result.message = "Network isolation requires elevated privileges";
    
    return result;
}

ResponseResult AutoResponse::backupFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->backupFileImpl(path);
}

ResponseResult AutoResponse::restoreFromBackup(const std::string& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    ResponseResult result;
    result.action = ResponseAction::RESTORE_FILE;
    result.target = path;
    result.timestamp = std::chrono::steady_clock::now();
    
    // Find most recent backup
    std::string filename = std::filesystem::path(path).filename().string();
    std::string latestBackup;
    std::filesystem::file_time_type latestTime;
    
    for (const auto& entry : std::filesystem::directory_iterator(impl_->config.backupDir)) {
        if (entry.path().filename().string().find(filename) == 0) {
            if (latestBackup.empty() || entry.last_write_time() > latestTime) {
                latestBackup = entry.path().string();
                latestTime = entry.last_write_time();
            }
        }
    }
    
    if (latestBackup.empty()) {
        result.success = false;
        result.message = "No backup found";
        return result;
    }
    
    try {
        std::filesystem::copy_file(latestBackup, path, 
                                    std::filesystem::copy_options::overwrite_existing);
        result.success = true;
        result.message = "File restored from backup";
        result.details["backup_path"] = latestBackup;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.message = std::string("Failed to restore: ") + e.what();
    }
    
    return result;
}

ResponseResult AutoResponse::rollbackChanges(const std::string& path,
                                              std::chrono::steady_clock::time_point since) {
    // This would integrate with file versioning system
    return restoreFromBackup(path);
}

void AutoResponse::addRule(const ResponseRule& rule) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->rules.push_back(rule);
}

void AutoResponse::removeRule(const std::string& ruleId) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->rules.erase(
        std::remove_if(impl_->rules.begin(), impl_->rules.end(),
                       [&](const ResponseRule& r) { return r.id == ruleId; }),
        impl_->rules.end()
    );
}

void AutoResponse::setRuleEnabled(const std::string& ruleId, bool enabled) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto& rule : impl_->rules) {
        if (rule.id == ruleId) {
            rule.enabled = enabled;
            break;
        }
    }
}

std::vector<ResponseRule> AutoResponse::getRules() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->rules;
}

ResponseRule AutoResponse::getRule(const std::string& ruleId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const auto& rule : impl_->rules) {
        if (rule.id == ruleId) return rule;
    }
    return ResponseRule{};
}

void AutoResponse::setAlertCallback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->alertCallback = callback;
}

void AutoResponse::setResponseCallback(ResponseCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->responseCallback = callback;
}

void AutoResponse::setConfirmationCallback(ConfirmationCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->confirmationCallback = callback;
}

std::vector<Alert> AutoResponse::getPendingAlerts() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<Alert> pending;
    for (const auto& alert : impl_->alerts) {
        if (!alert.acknowledged) {
            pending.push_back(alert);
        }
    }
    return pending;
}

void AutoResponse::acknowledgeAlert(const std::string& alertId) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto& alert : impl_->alerts) {
        if (alert.id == alertId) {
            alert.acknowledged = true;
            break;
        }
    }
}

void AutoResponse::clearAlerts() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->alerts.clear();
}

std::vector<std::string> AutoResponse::getQuarantinedFiles() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<std::string> files;
    for (const auto& [path, info] : impl_->quarantineMap) {
        files.push_back(path);
    }
    return files;
}

AutoResponse::QuarantineInfo AutoResponse::getQuarantineInfo(const std::string& quarantinePath) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->quarantineMap.find(quarantinePath);
    if (it != impl_->quarantineMap.end()) {
        return it->second;
    }
    return QuarantineInfo{};
}

uint64_t AutoResponse::getQuarantineSize() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->getQuarantineSizeImpl();
}

void AutoResponse::cleanQuarantine(std::chrono::hours maxAge) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> toRemove;
    
    for (const auto& [path, info] : impl_->quarantineMap) {
        if (now - info.quarantineTime > maxAge) {
            toRemove.push_back(path);
        }
    }
    
    for (const auto& path : toRemove) {
        std::filesystem::remove(path);
        std::filesystem::remove(path + ".meta");
        impl_->quarantineMap.erase(path);
    }
}

void AutoResponse::setConfig(const AutoResponseConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config = config;
}

AutoResponseConfig AutoResponse::getConfig() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->config;
}

AutoResponse::Stats AutoResponse::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

// ============================================================================
// DefaultRules Implementation
// ============================================================================

ResponseRule DefaultRules::getRansomwareRule() {
    ResponseRule rule;
    rule.id = "ransomware_response";
    rule.name = "Ransomware Response";
    rule.description = "Automatic response to ransomware detection";
    rule.minThreatLevel = ThreatLevel::HIGH;
    rule.threatTypes = {ThreatType::RANSOMWARE_PATTERN};
    rule.minConfidence = 0.8;
    rule.actions = {ResponseAction::ALERT, ResponseAction::BACKUP_FILE, ResponseAction::QUARANTINE_FILE};
    rule.priority = 100;
    rule.enabled = true;
    return rule;
}

ResponseRule DefaultRules::getMalwareRule() {
    ResponseRule rule;
    rule.id = "malware_response";
    rule.name = "Malware Response";
    rule.description = "Automatic response to malware detection";
    rule.minThreatLevel = ThreatLevel::HIGH;
    rule.threatTypes = {ThreatType::KNOWN_MALWARE_HASH, ThreatType::HIDDEN_EXECUTABLE};
    rule.minConfidence = 0.9;
    rule.actions = {ResponseAction::ALERT, ResponseAction::QUARANTINE_FILE};
    rule.priority = 90;
    rule.enabled = true;
    return rule;
}

ResponseRule DefaultRules::getCryptominerRule() {
    ResponseRule rule;
    rule.id = "cryptominer_response";
    rule.name = "Cryptominer Response";
    rule.description = "Response to cryptocurrency miner detection";
    rule.minThreatLevel = ThreatLevel::MEDIUM;
    rule.minConfidence = 0.7;
    rule.actions = {ResponseAction::ALERT, ResponseAction::LOG};
    rule.priority = 50;
    rule.enabled = true;
    return rule;
}

ResponseRule DefaultRules::getExfiltrationRule() {
    ResponseRule rule;
    rule.id = "exfiltration_response";
    rule.name = "Data Exfiltration Response";
    rule.description = "Response to data exfiltration attempts";
    rule.minThreatLevel = ThreatLevel::HIGH;
    rule.minConfidence = 0.75;
    rule.actions = {ResponseAction::ALERT, ResponseAction::LOG};
    rule.priority = 80;
    rule.enabled = true;
    return rule;
}

ResponseRule DefaultRules::getSuspiciousProcessRule() {
    ResponseRule rule;
    rule.id = "suspicious_process_response";
    rule.name = "Suspicious Process Response";
    rule.description = "Response to suspicious process detection";
    rule.minThreatLevel = ThreatLevel::MEDIUM;
    rule.threatTypes = {ThreatType::ANOMALOUS_BEHAVIOR};
    rule.minConfidence = 0.6;
    rule.actions = {ResponseAction::ALERT, ResponseAction::LOG};
    rule.priority = 40;
    rule.enabled = true;
    return rule;
}

std::vector<ResponseRule> DefaultRules::getAllDefaultRules() {
    return {
        getRansomwareRule(),
        getMalwareRule(),
        getCryptominerRule(),
        getExfiltrationRule(),
        getSuspiciousProcessRule()
    };
}

} // namespace Zer0
} // namespace SentinelFS
