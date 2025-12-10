#include "BehaviorAnalyzer.h"
#include <algorithm>
#include <set>

namespace SentinelFS {
namespace Zer0 {

BehaviorAnalyzer::BehaviorAnalyzer() = default;

BehaviorAnalyzer::~BehaviorAnalyzer() {
    stop();
}

void BehaviorAnalyzer::start(std::chrono::seconds windowSize) {
    windowSize_ = windowSize;
    running_ = true;
    
    pruneThread_ = std::thread(&BehaviorAnalyzer::pruneLoop, this);
}

void BehaviorAnalyzer::stop() {
    running_ = false;
    cv_.notify_all();
    
    if (pruneThread_.joinable()) {
        pruneThread_.join();
    }
}

void BehaviorAnalyzer::recordEvent(const BehaviorEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
    stats_.totalEvents++;
}

BehaviorAnalysis BehaviorAnalyzer::analyze() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    BehaviorAnalysis result;
    result.eventsInWindow = static_cast<int>(events_.size());
    result.uniqueFiles = countUniqueFiles();
    result.uniqueProcesses = countUniqueProcesses();
    
    if (events_.empty()) {
        result.pattern = BehaviorPattern::NORMAL;
        return result;
    }
    
    // Count event types
    int creates = countEventType("CREATE");
    int modifies = countEventType("MODIFY");
    int deletes = countEventType("DELETE");
    int renames = countEventType("RENAME");
    
    // Get process event counts
    auto processCounts = getProcessEventCounts();
    
    // Find most active process
    pid_t mostActivePid = 0;
    int maxCount = 0;
    for (const auto& [pid, count] : processCounts) {
        if (count > maxCount) {
            maxCount = count;
            mostActivePid = pid;
        }
    }
    
    // Check for mass modification (ransomware indicator)
    if (modifies >= massModificationThreshold_) {
        result.pattern = BehaviorPattern::MASS_MODIFICATION;
        result.threatLevel = ThreatLevel::HIGH;
        result.confidence = std::min(1.0, static_cast<double>(modifies) / massModificationThreshold_);
        result.description = "Mass file modification detected: " + std::to_string(modifies) + " files";
        stats_.anomaliesDetected++;
    }
    // Check for mass rename (ransomware indicator)
    else if (renames >= massRenameThreshold_) {
        result.pattern = BehaviorPattern::MASS_RENAME;
        result.threatLevel = ThreatLevel::HIGH;
        result.confidence = std::min(1.0, static_cast<double>(renames) / massRenameThreshold_);
        result.description = "Mass file rename detected: " + std::to_string(renames) + " files";
        stats_.anomaliesDetected++;
    }
    // Check for mass deletion
    else if (deletes >= massDeletionThreshold_) {
        result.pattern = BehaviorPattern::MASS_DELETION;
        result.threatLevel = ThreatLevel::MEDIUM;
        result.confidence = std::min(1.0, static_cast<double>(deletes) / massDeletionThreshold_);
        result.description = "Mass file deletion detected: " + std::to_string(deletes) + " files";
        stats_.anomaliesDetected++;
    }
    // Check for single process storm
    else if (maxCount > massModificationThreshold_ / 2 && result.uniqueProcesses <= 2) {
        result.pattern = BehaviorPattern::SINGLE_PROCESS_STORM;
        result.threatLevel = ThreatLevel::MEDIUM;
        result.confidence = 0.7;
        result.description = "Single process modifying many files";
        result.suspiciousPid = mostActivePid;
        result.processEventCount = maxCount;
        
        // Find process name
        for (const auto& event : events_) {
            if (event.pid == mostActivePid) {
                result.suspiciousProcess = event.processName;
                break;
            }
        }
        stats_.anomaliesDetected++;
    }
    // Check for extension change pattern
    else if (detectExtensionChangePattern()) {
        result.pattern = BehaviorPattern::EXTENSION_CHANGE;
        result.threatLevel = ThreatLevel::HIGH;
        result.confidence = 0.85;
        result.description = "File extension change pattern detected (possible ransomware)";
        stats_.anomaliesDetected++;
        stats_.ransomwarePatternsDetected++;
    }
    // Mass creation (might be extraction or copy)
    else if (creates >= massModificationThreshold_) {
        result.pattern = BehaviorPattern::MASS_CREATION;
        result.threatLevel = ThreatLevel::LOW;
        result.confidence = 0.5;
        result.description = "Mass file creation: " + std::to_string(creates) + " files";
    }
    else {
        result.pattern = BehaviorPattern::NORMAL;
        result.threatLevel = ThreatLevel::NONE;
    }
    
    return result;
}

void BehaviorAnalyzer::setMassModificationThreshold(int threshold) {
    massModificationThreshold_ = threshold;
}

void BehaviorAnalyzer::setMassRenameThreshold(int threshold) {
    massRenameThreshold_ = threshold;
}

void BehaviorAnalyzer::setMassDeletionThreshold(int threshold) {
    massDeletionThreshold_ = threshold;
}

bool BehaviorAnalyzer::detectRansomwarePattern() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Ransomware typically:
    // 1. Reads a file
    // 2. Writes encrypted version (high entropy)
    // 3. Renames with new extension (.encrypted, .locked, etc.)
    // 4. Deletes original
    
    // Check for extension change pattern
    if (detectExtensionChangePattern()) {
        stats_.ransomwarePatternsDetected++;
        return true;
    }
    
    // Check for high modify + rename ratio
    int modifies = countEventType("MODIFY");
    int renames = countEventType("RENAME");
    
    if (modifies > 10 && renames > 5) {
        double ratio = static_cast<double>(renames) / modifies;
        if (ratio > 0.3) {  // More than 30% of modifications followed by renames
            stats_.ransomwarePatternsDetected++;
            return true;
        }
    }
    
    return false;
}

std::vector<BehaviorEvent> BehaviorAnalyzer::getProcessEvents(pid_t pid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<BehaviorEvent> result;
    for (const auto& event : events_) {
        if (event.pid == pid) {
            result.push_back(event);
        }
    }
    return result;
}

std::vector<BehaviorEvent> BehaviorAnalyzer::getRecentEvents(std::chrono::seconds duration) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto cutoff = std::chrono::steady_clock::now() - duration;
    std::vector<BehaviorEvent> result;
    
    for (const auto& event : events_) {
        if (event.timestamp >= cutoff) {
            result.push_back(event);
        }
    }
    return result;
}

void BehaviorAnalyzer::pruneOldEvents() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto cutoff = std::chrono::steady_clock::now() - windowSize_;
    
    while (!events_.empty() && events_.front().timestamp < cutoff) {
        events_.pop_front();
    }
}

BehaviorAnalyzer::Stats BehaviorAnalyzer::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void BehaviorAnalyzer::pruneLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(10), [this] { return !running_.load(); });
        
        if (!running_) break;
        
        lock.unlock();
        pruneOldEvents();
    }
}

int BehaviorAnalyzer::countEventType(const std::string& type) const {
    int count = 0;
    for (const auto& event : events_) {
        if (event.eventType == type) {
            count++;
        }
    }
    return count;
}

int BehaviorAnalyzer::countUniqueFiles() const {
    std::set<std::string> files;
    for (const auto& event : events_) {
        files.insert(event.path);
    }
    return static_cast<int>(files.size());
}

int BehaviorAnalyzer::countUniqueProcesses() const {
    std::set<pid_t> processes;
    for (const auto& event : events_) {
        if (event.pid > 0) {
            processes.insert(event.pid);
        }
    }
    return static_cast<int>(processes.size());
}

std::map<pid_t, int> BehaviorAnalyzer::getProcessEventCounts() const {
    std::map<pid_t, int> counts;
    for (const auto& event : events_) {
        if (event.pid > 0) {
            counts[event.pid]++;
        }
    }
    return counts;
}

bool BehaviorAnalyzer::detectExtensionChangePattern() const {
    // Look for rename events that change file extensions
    std::set<std::string> newExtensions;
    int extensionChanges = 0;
    
    for (const auto& event : events_) {
        if (event.eventType == "RENAME") {
            // Check if extension changed
            size_t dotPos = event.path.rfind('.');
            if (dotPos != std::string::npos) {
                std::string ext = event.path.substr(dotPos);
                
                // Common ransomware extensions
                static const std::set<std::string> suspiciousExtensions = {
                    ".encrypted", ".locked", ".crypto", ".crypt",
                    ".enc", ".crypted", ".locky", ".cerber",
                    ".zepto", ".thor", ".aesir", ".zzzzz",
                    ".micro", ".xxx", ".ttt", ".ecc",
                    ".ezz", ".exx", ".xyz", ".aaa",
                    ".abc", ".ccc", ".vvv", ".xxx",
                    ".zzz", ".r5a", ".r4a", ".hermes",
                    ".WNCRY", ".WCRY", ".WNCRYT"
                };
                
                if (suspiciousExtensions.count(ext) > 0) {
                    return true;
                }
                
                newExtensions.insert(ext);
                extensionChanges++;
            }
        }
    }
    
    // If many files renamed to same new extension, suspicious
    if (extensionChanges > 5 && newExtensions.size() == 1) {
        return true;
    }
    
    return false;
}

} // namespace Zer0
} // namespace SentinelFS
