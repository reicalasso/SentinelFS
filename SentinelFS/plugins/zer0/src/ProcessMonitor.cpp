#include "ProcessMonitor.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <algorithm>
#include <regex>
#include <unistd.h>
#include <climits>

namespace SentinelFS {
namespace Zer0 {

// ============================================================================
// ProcessMonitor Implementation
// ============================================================================

class ProcessMonitor::Impl {
public:
    ProcessMonitorConfig config;
    std::map<pid_t, ProcessInfo> processes;
    ProcessEventCallback eventCallback;
    SuspiciousBehaviorCallback suspiciousCallback;
    Stats stats;
    
    std::atomic<bool> running{false};
    std::thread monitorThread;
    mutable std::mutex mutex;
    
    void monitorLoop() {
        while (running) {
            updateProcessList();
            checkSuspiciousBehaviors();
            std::this_thread::sleep_for(config.pollInterval);
        }
    }
    
    void updateProcessList() {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::set<pid_t> currentPids;
        
#ifdef __linux__
        DIR* procDir = opendir("/proc");
        if (!procDir) return;
        
        struct dirent* entry;
        while ((entry = readdir(procDir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;
            
            // Check if directory name is a number (PID)
            char* endptr;
            pid_t pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr != '\0' || pid <= 0) continue;
            
            currentPids.insert(pid);
            
            // Check if this is a new process
            bool isNew = (processes.find(pid) == processes.end());
            
            // Update process info
            ProcessInfo info = readProcessInfo(pid);
            if (info.pid > 0) {
                if (isNew) {
                    info.startTime = std::chrono::steady_clock::now();
                    stats.processCreations++;
                    
                    if (eventCallback) {
                        ProcessEvent event;
                        event.type = ProcessEventType::CREATED;
                        event.process = info;
                        event.timestamp = std::chrono::steady_clock::now();
                        eventCallback(event);
                    }
                }
                info.lastSeen = std::chrono::steady_clock::now();
                processes[pid] = info;
            }
        }
        closedir(procDir);
        
        // Check for terminated processes
        std::vector<pid_t> terminated;
        for (const auto& [pid, info] : processes) {
            if (currentPids.find(pid) == currentPids.end()) {
                terminated.push_back(pid);
                stats.processTerminations++;
                
                if (eventCallback) {
                    ProcessEvent event;
                    event.type = ProcessEventType::TERMINATED;
                    event.process = info;
                    event.timestamp = std::chrono::steady_clock::now();
                    eventCallback(event);
                }
            }
        }
        
        for (pid_t pid : terminated) {
            processes.erase(pid);
        }
#endif
        
        stats.processesTracked = processes.size();
        stats.lastUpdate = std::chrono::steady_clock::now();
    }
    
    ProcessInfo readProcessInfo(pid_t pid) {
        ProcessInfo info;
        info.pid = pid;
        
#ifdef __linux__
        std::string procPath = "/proc/" + std::to_string(pid);
        
        // Read /proc/[pid]/stat
        std::ifstream statFile(procPath + "/stat");
        if (statFile) {
            std::string line;
            std::getline(statFile, line);
            
            // Parse stat file
            size_t nameStart = line.find('(');
            size_t nameEnd = line.rfind(')');
            if (nameStart != std::string::npos && nameEnd != std::string::npos) {
                info.name = line.substr(nameStart + 1, nameEnd - nameStart - 1);
                
                // Parse remaining fields after the name
                std::istringstream iss(line.substr(nameEnd + 2));
                char state;
                iss >> state >> info.ppid;
            }
        }
        
        // Read /proc/[pid]/cmdline
        std::ifstream cmdlineFile(procPath + "/cmdline");
        if (cmdlineFile) {
            std::string cmdline;
            std::getline(cmdlineFile, cmdline, '\0');
            
            // Replace null bytes with spaces
            std::string fullCmdline;
            char c;
            while (cmdlineFile.get(c)) {
                if (c == '\0') fullCmdline += ' ';
                else fullCmdline += c;
            }
            info.cmdline = cmdline + fullCmdline;
        }
        
        // Read /proc/[pid]/exe (symlink to executable)
        char exePath[PATH_MAX];
        ssize_t len = readlink((procPath + "/exe").c_str(), exePath, sizeof(exePath) - 1);
        if (len > 0) {
            exePath[len] = '\0';
            info.path = exePath;
        }
        
        // Read /proc/[pid]/cwd
        char cwdPath[PATH_MAX];
        len = readlink((procPath + "/cwd").c_str(), cwdPath, sizeof(cwdPath) - 1);
        if (len > 0) {
            cwdPath[len] = '\0';
            info.cwd = cwdPath;
        }
        
        // Read /proc/[pid]/status for UID/GID
        std::ifstream statusFile(procPath + "/status");
        if (statusFile) {
            std::string line;
            while (std::getline(statusFile, line)) {
                if (line.find("Uid:") == 0) {
                    std::istringstream iss(line.substr(4));
                    iss >> info.uid;
                } else if (line.find("Gid:") == 0) {
                    std::istringstream iss(line.substr(4));
                    iss >> info.gid;
                } else if (line.find("VmRSS:") == 0) {
                    std::istringstream iss(line.substr(6));
                    uint64_t kb;
                    iss >> kb;
                    info.memoryBytes = kb * 1024;
                } else if (line.find("VmSize:") == 0) {
                    std::istringstream iss(line.substr(7));
                    uint64_t kb;
                    iss >> kb;
                    info.virtualMemoryBytes = kb * 1024;
                }
            }
        }
        
        // Get username from UID
        struct passwd* pw = getpwuid(info.uid);
        if (pw) {
            info.username = pw->pw_name;
        }
        
        // Check if elevated (root)
        info.isElevated = (info.uid == 0);
        
        // Count file descriptors
        std::string fdPath = procPath + "/fd";
        if (std::filesystem::exists(fdPath)) {
            info.fileDescriptorCount = std::distance(
                std::filesystem::directory_iterator(fdPath),
                std::filesystem::directory_iterator{}
            );
        }
        
        // Read I/O stats
        std::ifstream ioFile(procPath + "/io");
        if (ioFile) {
            std::string line;
            while (std::getline(ioFile, line)) {
                if (line.find("read_bytes:") == 0) {
                    info.readBytes = std::stoull(line.substr(12));
                } else if (line.find("write_bytes:") == 0) {
                    info.writeBytes = std::stoull(line.substr(13));
                }
            }
        }
        
        // Get children
        std::string taskPath = procPath + "/task/" + std::to_string(pid) + "/children";
        std::ifstream childrenFile(taskPath);
        if (childrenFile) {
            pid_t childPid;
            while (childrenFile >> childPid) {
                info.children.push_back(childPid);
            }
        }
#endif
        
        return info;
    }
    
    void checkSuspiciousBehaviors() {
        std::lock_guard<std::mutex> lock(mutex);
        
        for (auto& [pid, info] : processes) {
            // Skip whitelisted
            if (config.whitelistedPids.count(pid) ||
                config.whitelistedProcesses.count(info.name) ||
                config.whitelistedPaths.count(info.path)) {
                continue;
            }
            
            std::vector<SuspiciousBehavior> behaviors;
            
            // Check for known malware
            if (MaliciousPatterns::isKnownMalware(info.name)) {
                SuspiciousBehavior b;
                b.type = "KNOWN_MALWARE";
                b.description = "Process name matches known malware";
                b.severity = 0.9;
                b.process = info;
                behaviors.push_back(b);
            }
            
            // Check suspicious command line
            if (MaliciousPatterns::isSuspiciousCommandLine(info.cmdline)) {
                SuspiciousBehavior b;
                b.type = "SUSPICIOUS_CMDLINE";
                b.description = "Suspicious command line detected";
                b.severity = 0.7;
                b.process = info;
                b.indicators = MaliciousPatterns::getSuspicionReasons(info);
                behaviors.push_back(b);
            }
            
            // Check suspicious path
            if (MaliciousPatterns::isSuspiciousPath(info.path)) {
                SuspiciousBehavior b;
                b.type = "SUSPICIOUS_PATH";
                b.description = "Process running from suspicious location";
                b.severity = 0.6;
                b.process = info;
                behaviors.push_back(b);
            }
            
            // Check resource usage
            if (config.trackResourceUsage) {
                if (info.cpuPercent > config.cpuThreshold) {
                    SuspiciousBehavior b;
                    b.type = "HIGH_CPU";
                    b.description = "Abnormally high CPU usage";
                    b.severity = 0.5;
                    b.process = info;
                    behaviors.push_back(b);
                }
                
                if (info.memoryBytes > config.memoryThreshold) {
                    SuspiciousBehavior b;
                    b.type = "HIGH_MEMORY";
                    b.description = "Abnormally high memory usage";
                    b.severity = 0.5;
                    b.process = info;
                    behaviors.push_back(b);
                }
            }
            
            // Check for too many file descriptors
            if (info.fileDescriptorCount > config.maxFileDescriptors) {
                SuspiciousBehavior b;
                b.type = "MANY_FDS";
                b.description = "Too many open file descriptors";
                b.severity = 0.4;
                b.process = info;
                behaviors.push_back(b);
            }
            
            // Check for too many children
            if (static_cast<int>(info.children.size()) > config.maxChildren) {
                SuspiciousBehavior b;
                b.type = "MANY_CHILDREN";
                b.description = "Process spawned too many children";
                b.severity = 0.6;
                b.process = info;
                behaviors.push_back(b);
            }
            
            // Report suspicious behaviors
            if (!behaviors.empty()) {
                info.isSuspicious = true;
                stats.suspiciousDetected += behaviors.size();
                
                if (suspiciousCallback) {
                    for (const auto& b : behaviors) {
                        suspiciousCallback(b);
                    }
                }
            }
        }
    }
};

ProcessMonitor::ProcessMonitor() : impl_(std::make_unique<Impl>()) {}

ProcessMonitor::~ProcessMonitor() {
    stop();
}

bool ProcessMonitor::initialize(const ProcessMonitorConfig& config) {
    impl_->config = config;
    impl_->stats.startTime = std::chrono::steady_clock::now();
    return true;
}

bool ProcessMonitor::start() {
    if (impl_->running) return true;
    
    impl_->running = true;
    impl_->monitorThread = std::thread(&Impl::monitorLoop, impl_.get());
    return true;
}

void ProcessMonitor::stop() {
    impl_->running = false;
    if (impl_->monitorThread.joinable()) {
        impl_->monitorThread.join();
    }
}

bool ProcessMonitor::isRunning() const {
    return impl_->running;
}

ProcessInfo ProcessMonitor::getProcessInfo(pid_t pid) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->processes.find(pid);
    if (it != impl_->processes.end()) {
        return it->second;
    }
    return ProcessInfo{};
}

std::vector<ProcessInfo> ProcessMonitor::getAllProcesses() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<ProcessInfo> result;
    for (const auto& [pid, info] : impl_->processes) {
        result.push_back(info);
    }
    return result;
}

std::vector<ProcessInfo> ProcessMonitor::getProcessTree(pid_t rootPid) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<ProcessInfo> result;
    
    std::function<void(pid_t)> collectTree = [&](pid_t pid) {
        auto it = impl_->processes.find(pid);
        if (it != impl_->processes.end()) {
            result.push_back(it->second);
            for (pid_t child : it->second.children) {
                collectTree(child);
            }
        }
    };
    
    collectTree(rootPid);
    return result;
}

std::vector<ProcessInfo> ProcessMonitor::getChildren(pid_t pid) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<ProcessInfo> result;
    
    auto it = impl_->processes.find(pid);
    if (it != impl_->processes.end()) {
        for (pid_t child : it->second.children) {
            auto childIt = impl_->processes.find(child);
            if (childIt != impl_->processes.end()) {
                result.push_back(childIt->second);
            }
        }
    }
    
    return result;
}

std::vector<ProcessInfo> ProcessMonitor::getParentChain(pid_t pid) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<ProcessInfo> result;
    
    pid_t current = pid;
    while (current > 1) {
        auto it = impl_->processes.find(current);
        if (it == impl_->processes.end()) break;
        
        result.push_back(it->second);
        current = it->second.ppid;
    }
    
    return result;
}

std::vector<ProcessInfo> ProcessMonitor::findByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<ProcessInfo> result;
    
    for (const auto& [pid, info] : impl_->processes) {
        if (info.name == name || info.name.find(name) != std::string::npos) {
            result.push_back(info);
        }
    }
    
    return result;
}

std::vector<ProcessInfo> ProcessMonitor::findByPath(const std::string& path) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<ProcessInfo> result;
    
    for (const auto& [pid, info] : impl_->processes) {
        if (info.path == path || info.path.find(path) != std::string::npos) {
            result.push_back(info);
        }
    }
    
    return result;
}

bool ProcessMonitor::isSuspicious(pid_t pid) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->processes.find(pid);
    return it != impl_->processes.end() && it->second.isSuspicious;
}

std::vector<SuspiciousBehavior> ProcessMonitor::getSuspiciousBehaviors(pid_t pid) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<SuspiciousBehavior> result;
    
    auto it = impl_->processes.find(pid);
    if (it == impl_->processes.end()) return result;
    
    const auto& info = it->second;
    auto reasons = MaliciousPatterns::getSuspicionReasons(info);
    
    for (const auto& reason : reasons) {
        SuspiciousBehavior b;
        b.type = "SUSPICIOUS";
        b.description = reason;
        b.process = info;
        result.push_back(b);
    }
    
    return result;
}

void ProcessMonitor::setEventCallback(ProcessEventCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->eventCallback = callback;
}

void ProcessMonitor::setSuspiciousBehaviorCallback(SuspiciousBehaviorCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->suspiciousCallback = callback;
}

void ProcessMonitor::whitelistProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config.whitelistedProcesses.insert(name);
}

void ProcessMonitor::whitelistPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config.whitelistedPaths.insert(path);
}

void ProcessMonitor::whitelistPid(pid_t pid) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config.whitelistedPids.insert(pid);
}

void ProcessMonitor::removeFromWhitelist(const std::string& name) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config.whitelistedProcesses.erase(name);
    impl_->config.whitelistedPaths.erase(name);
}

void ProcessMonitor::removeFromWhitelist(pid_t pid) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config.whitelistedPids.erase(pid);
}

void ProcessMonitor::setConfig(const ProcessMonitorConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config = config;
}

ProcessMonitorConfig ProcessMonitor::getConfig() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->config;
}

ProcessMonitor::Stats ProcessMonitor::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

// ============================================================================
// ProcessTreeAnalyzer Implementation
// ============================================================================

std::vector<SuspiciousBehavior> ProcessTreeAnalyzer::analyzeTree(const std::vector<ProcessInfo>& tree) {
    std::vector<SuspiciousBehavior> results;
    
    for (size_t i = 0; i < tree.size(); ++i) {
        const auto& process = tree[i];
        
        // Check for suspicious parent-child relationships
        if (i > 0) {
            const auto& parent = tree[i - 1];
            
            if (detectInjection(parent, process)) {
                SuspiciousBehavior b;
                b.type = "PROCESS_INJECTION";
                b.description = "Possible process injection detected";
                b.severity = 0.9;
                b.process = process;
                results.push_back(b);
            }
        }
        
        if (detectHollowing(process)) {
            SuspiciousBehavior b;
            b.type = "PROCESS_HOLLOWING";
            b.description = "Possible process hollowing detected";
            b.severity = 0.95;
            b.process = process;
            results.push_back(b);
        }
    }
    
    if (detectPrivilegeEscalation(tree)) {
        SuspiciousBehavior b;
        b.type = "PRIVILEGE_ESCALATION";
        b.description = "Privilege escalation chain detected";
        b.severity = 0.9;
        b.process = tree.back();
        results.push_back(b);
    }
    
    return results;
}

bool ProcessTreeAnalyzer::detectInjection(const ProcessInfo& parent, const ProcessInfo& child) {
    // Suspicious if child has different path but same name as a system process
    static const std::set<std::string> systemProcesses = {
        "svchost", "lsass", "csrss", "winlogon", "explorer",
        "systemd", "init", "bash", "sh"
    };
    
    if (systemProcesses.count(child.name) && 
        child.path.find("/usr") == std::string::npos &&
        child.path.find("/bin") == std::string::npos &&
        child.path.find("System32") == std::string::npos) {
        return true;
    }
    
    return false;
}

bool ProcessTreeAnalyzer::detectHollowing(const ProcessInfo& process) {
    // Check if process image doesn't match expected location
    // This is a simplified check
    if (process.name == "svchost" && process.path.find("System32") == std::string::npos) {
        return true;
    }
    
    return false;
}

bool ProcessTreeAnalyzer::detectPrivilegeEscalation(const std::vector<ProcessInfo>& chain) {
    if (chain.size() < 2) return false;
    
    // Check if a non-root process spawned a root process
    for (size_t i = 1; i < chain.size(); ++i) {
        if (!chain[i - 1].isElevated && chain[i].isElevated) {
            // Check if this is expected (sudo, su, etc.)
            if (chain[i].name != "sudo" && chain[i].name != "su" &&
                chain[i].name != "pkexec" && chain[i].name != "doas") {
                return true;
            }
        }
    }
    
    return false;
}

double ProcessTreeAnalyzer::calculateRiskScore(const ProcessInfo& process) {
    double score = 0.0;
    
    // Base score from various factors
    if (process.isElevated) score += 0.1;
    if (process.isHidden) score += 0.3;
    if (process.isSuspicious) score += 0.2;
    
    // Check path
    if (MaliciousPatterns::isSuspiciousPath(process.path)) score += 0.2;
    
    // Check command line
    if (MaliciousPatterns::isSuspiciousCommandLine(process.cmdline)) score += 0.2;
    
    // Check for known malware
    if (MaliciousPatterns::isKnownMalware(process.name)) score += 0.5;
    
    return std::min(1.0, score);
}

std::vector<std::string> ProcessTreeAnalyzer::getAttackPatterns(const ProcessInfo& process) {
    return MaliciousPatterns::getSuspicionReasons(process);
}

// ============================================================================
// MaliciousPatterns Implementation
// ============================================================================

bool MaliciousPatterns::isKnownMalware(const std::string& name) {
    static const std::set<std::string> malwareNames = {
        "mimikatz", "lazagne", "procdump", "pwdump",
        "gsecdump", "wce", "fgdump", "cachedump",
        "lsadump", "secretsdump", "kerberoast",
        "bloodhound", "sharphound", "rubeus",
        "covenant", "empire", "metasploit",
        "cobalt", "beacon", "meterpreter"
    };
    
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (const auto& malware : malwareNames) {
        if (lower.find(malware) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool MaliciousPatterns::isSuspiciousCommandLine(const std::string& cmdline) {
    static const std::vector<std::regex> suspiciousPatterns = {
        std::regex("-enc\\s+[A-Za-z0-9+/=]+", std::regex::icase),  // Encoded PowerShell
        std::regex("IEX\\s*\\(", std::regex::icase),               // Invoke-Expression
        std::regex("DownloadString", std::regex::icase),
        std::regex("FromBase64String", std::regex::icase),
        std::regex("\\|\\s*bash", std::regex::icase),              // Pipe to bash
        std::regex("curl.*\\|.*sh", std::regex::icase),            // Curl pipe to shell
        std::regex("wget.*\\|.*sh", std::regex::icase),
        std::regex("/dev/tcp/", std::regex::icase),                // Bash reverse shell
        std::regex("nc\\s+-e", std::regex::icase),                 // Netcat shell
        std::regex("rm\\s+-rf\\s+/", std::regex::icase),           // Dangerous rm
        std::regex("chmod\\s+777", std::regex::icase),
        std::regex(":(){ :|:& };:", std::regex::icase)             // Fork bomb
    };
    
    for (const auto& pattern : suspiciousPatterns) {
        if (std::regex_search(cmdline, pattern)) {
            return true;
        }
    }
    
    return false;
}

bool MaliciousPatterns::isSuspiciousPath(const std::string& path) {
    static const std::vector<std::string> suspiciousPaths = {
        "/tmp/",
        "/var/tmp/",
        "/dev/shm/",
        "/.hidden",
        "/...",
        "AppData/Local/Temp",
        "AppData/Roaming",
        "ProgramData"
    };
    
    for (const auto& suspicious : suspiciousPaths) {
        if (path.find(suspicious) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

std::vector<std::string> MaliciousPatterns::getSuspicionReasons(const ProcessInfo& process) {
    std::vector<std::string> reasons;
    
    if (isKnownMalware(process.name)) {
        reasons.push_back("Process name matches known malware: " + process.name);
    }
    
    if (isSuspiciousCommandLine(process.cmdline)) {
        reasons.push_back("Suspicious command line pattern detected");
    }
    
    if (isSuspiciousPath(process.path)) {
        reasons.push_back("Running from suspicious location: " + process.path);
    }
    
    if (process.isHidden) {
        reasons.push_back("Process is attempting to hide");
    }
    
    if (process.isElevated && process.path.find("/tmp") != std::string::npos) {
        reasons.push_back("Elevated process running from temp directory");
    }
    
    return reasons;
}

const std::vector<std::string>& MaliciousPatterns::getKnownMalwareNames() {
    static const std::vector<std::string> names = {
        "mimikatz", "lazagne", "procdump", "pwdump",
        "gsecdump", "wce", "fgdump", "cachedump",
        "lsadump", "secretsdump", "kerberoast",
        "bloodhound", "sharphound", "rubeus",
        "covenant", "empire", "metasploit",
        "cobalt", "beacon", "meterpreter"
    };
    return names;
}

const std::vector<std::string>& MaliciousPatterns::getSuspiciousCommandPatterns() {
    static const std::vector<std::string> patterns = {
        "-enc ", "IEX(", "DownloadString", "FromBase64String",
        "| bash", "| sh", "/dev/tcp/", "nc -e",
        "rm -rf /", "chmod 777", ":(){ :|:& };:"
    };
    return patterns;
}

} // namespace Zer0
} // namespace SentinelFS
