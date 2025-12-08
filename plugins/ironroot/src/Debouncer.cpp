#include "Debouncer.h"
#include "IronRootPlugin.h"
#include "Logger.h"
#include <algorithm>
#include <regex>
#include <filesystem>

namespace SentinelFS {
namespace IronRoot {

Debouncer::Debouncer() = default;

Debouncer::~Debouncer() {
    stop();
}

void Debouncer::start(const DebounceConfig& config, OutputCallback callback) {
    config_ = config;
    outputCallback_ = callback;
    
    running_ = true;
    processThread_ = std::thread(&Debouncer::processLoop, this);
    
    Logger::instance().log(LogLevel::DEBUG, "Debouncer started with " + 
                           std::to_string(config.window.count()) + "ms window", "Debouncer");
}

void Debouncer::stop() {
    running_ = false;
    cv_.notify_all();
    
    if (processThread_.joinable()) {
        processThread_.join();
    }
    
    // Flush remaining events
    flush();
}

void Debouncer::addEvent(const WatchEvent& event) {
    auto now = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.eventsReceived++;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check for atomic write pattern
    if (config_.detectAtomicWrites && detectAtomicWrite(event)) {
        return;  // Event will be handled as atomic write
    }
    
    auto it = pendingEvents_.find(event.path);
    if (it != pendingEvents_.end()) {
        // Update existing pending event
        it->second.lastSeen = now;
        it->second.count++;
        
        // Coalesce event types
        if (config_.coalesceModifies) {
            // If we have Create followed by Modify, keep as Create
            // If we have Modify followed by Delete, change to Delete
            if (it->second.event.type == WatchEventType::Create && 
                event.type == WatchEventType::Modify) {
                // Keep as Create
            } else if (event.type == WatchEventType::Delete) {
                it->second.event.type = WatchEventType::Delete;
            }
        }
        
        {
            std::lock_guard<std::mutex> slock(statsMutex_);
            stats_.eventsCoalesced++;
        }
    } else {
        // New pending event
        PendingEvent pending;
        pending.event = event;
        pending.firstSeen = now;
        pending.lastSeen = now;
        pending.count = 1;
        pendingEvents_[event.path] = pending;
    }
    
    cv_.notify_one();
}

void Debouncer::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [path, pending] : pendingEvents_) {
        emitEvent(pending);
    }
    pendingEvents_.clear();
}

void Debouncer::setBatchCallback(BatchCallback callback) {
    batchCallback_ = callback;
}

Debouncer::Stats Debouncer::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void Debouncer::processLoop() {
    while (running_) {
        std::vector<std::string> toEmit;
        auto now = std::chrono::steady_clock::now();
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // Wait for events or timeout
            cv_.wait_for(lock, config_.window, [this]() {
                return !running_ || !pendingEvents_.empty();
            });
            
            if (!running_) break;
            
            // Check which events are ready to emit
            for (auto it = pendingEvents_.begin(); it != pendingEvents_.end();) {
                auto& pending = it->second;
                auto timeSinceFirst = now - pending.firstSeen;
                auto timeSinceLast = now - pending.lastSeen;
                
                bool shouldEmit = false;
                
                // Emit if:
                // 1. Debounce window has passed since last event
                // 2. Max delay has been reached since first event
                if (timeSinceLast >= config_.window) {
                    shouldEmit = true;
                } else if (timeSinceFirst >= config_.maxDelay) {
                    shouldEmit = true;
                }
                
                if (shouldEmit) {
                    toEmit.push_back(it->first);
                }
                ++it;
            }
        }
        
        // Emit events outside lock
        for (const auto& path : toEmit) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pendingEvents_.find(path);
            if (it != pendingEvents_.end()) {
                emitEvent(it->second);
                pendingEvents_.erase(it);
            }
        }
    }
}

void Debouncer::emitEvent(const PendingEvent& pending) {
    if (outputCallback_) {
        outputCallback_(pending.event, pending.isAtomicWrite);
    }
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.eventsEmitted++;
        if (pending.isAtomicWrite) {
            stats_.atomicWritesDetected++;
        }
    }
}

bool Debouncer::detectAtomicWrite(const WatchEvent& event) {
    // Atomic write pattern:
    // 1. Create temp file (e.g., .file.tmp, file~, #file#)
    // 2. Write to temp file
    // 3. Rename temp to target
    
    if (event.type == WatchEventType::Create && isTempFile(event.path)) {
        // Track this temp file
        // Try to guess the target path
        std::string target = event.path;
        
        // Remove common temp suffixes/prefixes
        if (target.size() > 4 && target.substr(target.size() - 4) == ".tmp") {
            target = target.substr(0, target.size() - 4);
        } else if (target.size() > 1 && target.back() == '~') {
            target = target.substr(0, target.size() - 1);
        } else if (target.size() > 2 && target[0] == '.' && target.back() == '.') {
            // .file. pattern
            target = target.substr(1, target.size() - 2);
        }
        
        tempToTarget_[event.path] = target;
        return false;  // Still process the create
    }
    
    if (event.type == WatchEventType::Rename && event.oldPath.has_value()) {
        // Check if this is a temp → target rename
        auto it = tempToTarget_.find(event.oldPath.value());
        if (it != tempToTarget_.end()) {
            // This is an atomic write!
            // Emit a single Modify event for the target
            WatchEvent modifyEvent;
            modifyEvent.type = WatchEventType::Modify;
            modifyEvent.path = event.path;
            modifyEvent.isDirectory = event.isDirectory;
            modifyEvent.pid = event.pid;
            modifyEvent.processName = event.processName;
            
            PendingEvent pending;
            pending.event = modifyEvent;
            pending.firstSeen = std::chrono::steady_clock::now();
            pending.lastSeen = pending.firstSeen;
            pending.count = 1;
            pending.isAtomicWrite = true;
            pending.tempPath = event.oldPath.value();
            
            emitEvent(pending);
            
            tempToTarget_.erase(it);
            
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.atomicWritesDetected++;
            }
            
            return true;  // Don't process as normal rename
        }
    }
    
    // Clean up old temp file tracking (delete events)
    if (event.type == WatchEventType::Delete) {
        tempToTarget_.erase(event.path);
    }
    
    return false;
}

bool Debouncer::isTempFile(const std::string& path) const {
    std::string filename = std::filesystem::path(path).filename().string();
    
    // Common temp file patterns
    static const std::vector<std::regex> patterns = {
        std::regex(R"(^\..*\.swp$)"),           // vim swap
        std::regex(R"(^\..*\.swo$)"),           // vim swap overflow
        std::regex(R"(^#.*#$)"),                // emacs auto-save
        std::regex(R"(.*~$)"),                  // backup files
        std::regex(R"(.*\.tmp$)"),              // .tmp suffix
        std::regex(R"(.*\.temp$)"),             // .temp suffix
        std::regex(R"(^\.~.*$)"),               // hidden temp
        std::regex(R"(.*\.part$)"),             // partial download
        std::regex(R"(.*\.crdownload$)"),       // chrome download
        std::regex(R"(^\.goutputstream-.*)"),   // GNOME temp
        std::regex(R"(^\.nfs.*)"),              // NFS temp
        std::regex(R"(^4913$)"),                // vim temp
    };
    
    for (const auto& pattern : patterns) {
        if (std::regex_match(filename, pattern)) {
            return true;
        }
    }
    
    return false;
}

} // namespace IronRoot
} // namespace SentinelFS
